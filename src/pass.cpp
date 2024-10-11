#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <cstdint>

using namespace llvm;

namespace {

struct GlobalizeGlobalUnnamedPointersPass : public PassInfoMixin<GlobalizeGlobalUnnamedPointersPass> {

bool shouldGlobalizeVariable(const GlobalVariable &gv) {
    if (gv.hasExternalLinkage() && gv.isDeclaration())
      return false;

    if (gv.isConstant())
      return false;

    if (!gv.getValueType()->isPointerTy())
      return false;

    return true;
}

PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam) {
    bool anyGlobalsModified = false;

    Function *globalifyFunc = m.getFunction("globalify");
    if (!globalifyFunc) {
        errs() << "[GlobalizeGlobalUnnamedPointersPass] -- globalify function not found. exiting early.\n";
        return PreservedAnalyses::all();
    }

    LLVMContext &cx = m.getContext();

    FunctionType *ctorTy = FunctionType::get(Type::getVoidTy(cx), false);
    Function *ctor = Function::Create(ctorTy, Function::InternalLinkage, "global_ptr_transform_ctor", &m);
    
    BasicBlock *bb = BasicBlock::Create(cx, "entry", ctor);
    IRBuilder<> builder(bb);

    PointerType *int8PtrType = PointerType::get(Type::getInt8Ty(cx), 0);
    FunctionType *globalifyType = FunctionType::get(int8PtrType, {int8PtrType}, false);
    for (GlobalVariable &gv : m.globals()) {
        if(shouldGlobalizeVariable(gv)) {
            if(auto *gvPointee = dyn_cast<GlobalVariable>(gv.getInitializer())) {
                anyGlobalsModified = true;
                Value *gvPointeePtr = builder.CreateBitCast(gvPointee, int8PtrType);
                CallInst *globalifiedPointeePtr = builder.CreateCall(globalifyFunc, {gvPointeePtr});
                Value *castedPtr = builder.CreateBitCast(globalifiedPointeePtr, gv.getValueType());
                builder.CreateStore(castedPtr, &gv);
            }
        }
    }

    builder.CreateRetVoid();

    appendToGlobalCtors(m, ctor, 0);

    return anyGlobalsModified ? PreservedAnalyses::none()
                    : PreservedAnalyses::all();
}

};

///////////////////////////////////////////////////////////////////////////////////////////////////

struct UnwrapLibFnCallParamsPass : public PassInfoMixin<UnwrapLibFnCallParamsPass> {

PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam) {
    bool anyOperandsAltered = false;

    Function *deglobalifyFunc = m.getFunction("deglobalify");
    if (!deglobalifyFunc) {
        errs() << "[UNWRAPLIBFNCALLPARAMS PASS] -- deglobalify function not found. exiting early.\n";
        return PreservedAnalyses::all();
    }

    const TargetLibraryInfo *tli; 
    LibFunc inbuiltFunc;
    std::set<StringRef> builtins;
    for (Function &f : m) {
        if(tli->getLibFunc(f, inbuiltFunc)) {
            builtins.insert(f.getFunction().getName());
        }
    }


    IRBuilder<> builder(m.getContext());

    uint64_t direct_calls = 0;
    uint64_t indirect_calls = 0;

    for (Function &f : m) {
        if (f.getName() == "__pando__replace_load_int64" ||
            f.getName() == "__pando__replace_load_int32" ||
            f.getName() == "__pando__replace_load_int16" ||
            f.getName() == "__pando__replace_load_int8" ||
            f.getName() == "__pando__replace_load_float32" ||
            f.getName() == "__pando__replace_load_float64" ||
            f.getName() == "__pando__replace_load_ptr" ||
            f.getName() == "__pando__replace_load_vector" ||
            f.getName() == "__pando__replace_store_int64" ||
            f.getName() == "__pando__replace_store_int32" ||
            f.getName() == "__pando__replace_store_int16" ||
            f.getName() == "__pando__replace_store_int8" ||
            f.getName() == "__pando__replace_store_float32" ||
            f.getName() == "__pando__replace_store_float64" ||
            f.getName() == "__pando__replace_store_ptr" ||
            f.getName() == "__pando__replace_store_vector" ||
            f.getName() == "check_if_global" || 
            f.getName() == "deglobalify" ||
            f.getName() == "globalify") {
            continue;
        }

        // iterate over all instructions in the function
        for (BasicBlock &bb : f) {
            for (Instruction &instr : bb) {
                if (CallInst* callInstr = dyn_cast<CallInst>(&instr)) {

                    // errs() << "\ninstr is " << instr << "\n";
                    
                    Function* calledFunction = callInstr->getCalledFunction();
                    if (!calledFunction) {
                        // for now, let's not modify indirect function calls...
                        indirect_calls++;
                        continue;
                    } else {
                        direct_calls++;
                    }

                    StringRef funcName = calledFunction->getName();
                    
                    bool print = false;

                    if (m.getFunction(funcName) && 
                        calledFunction->getIntrinsicID() == 0 && 
                        builtins.count(funcName) == 0) {
                        // if it's 
                        //  - in the module 
                        //  - it isn't an intrinsic (id == 0) 
                        //  - it isn't a library function
                        // we will skip this function.
                        continue;
                    } 

                    // errs() << "we will modify this instr\n"; 

                    // else, this is a call to a library function or intrinsic.

                    uint64_t numOperands = instr.getNumOperands();

                    for(uint64_t operandIdx = 0; operandIdx < numOperands - 1; operandIdx++) {
                        Value* operand = instr.getOperand(operandIdx);
                        if(print) errs() << "  operand #" << operandIdx << " is " << *operand << "\n";
                        if(operand->getType()->isPointerTy()) {
                            if(print) errs() << "  it's a pointer type too. downgrade.\n"; 
                            builder.SetInsertPoint(&instr);
                            Value* deglobalified_ptr = builder
                                .CreateCall(deglobalifyFunc, {operand}, "lib_deglobalified_ptr");
                            instr.setOperand(operandIdx, deglobalified_ptr);
                            anyOperandsAltered = true;
                        }
                        if(print) errs() << "instr is now" << instr << "\n";
                    }
                }
            }
        }
    }

    errs() << "direct calls: " << direct_calls << "\n";
    errs() << "indirect calls: " << indirect_calls << "\n";

    return anyOperandsAltered ? PreservedAnalyses::none()
                            : PreservedAnalyses::all();
}

};

///////////////////////////////////////////////////////////////////////////////////////////////////

struct GlobalizePass : public PassInfoMixin<GlobalizePass> {

static bool processInstruction(Module &m, IRBuilder<> &builder,
                               Function *globalifyFunc, Function *deglobalifyFunc, Instruction &instr) {
    bool oneConstGlobalified = false;

    unsigned numOperands = instr.getNumOperands();
    uint64_t instrOpcode = instr.getOpcode();

    // iterate through the operands
    for (unsigned operandIndex = 0; operandIndex < numOperands; operandIndex++) {
        Value *operand = instr.getOperand(operandIndex);
        assert(operand);

        Type *type = operand->getType();
        assert(type);

        if (auto *constExpr = dyn_cast<ConstantExpr>(operand)) {
            // handle the constexpr-as-an-operand case. 
            // the constexpr can contain references to globals.

            // so far, we only care about ICmp and GEP instructions as constexpr operands.
                // *there likely will be more.*
            if (constExpr->getOpcode() == Instruction::ICmp) {

                Value* lhs = constExpr->getOperand(0);
                Value* rhs = constExpr->getOperand(1);

                if (!isa<GlobalVariable>(lhs) && !isa<GlobalVariable>(rhs)) {
                    continue;
                }

                builder.SetInsertPoint(&instr);

                if (isa<GlobalVariable>(lhs)) {
                    lhs = builder.CreateCall(globalifyFunc, {lhs}, "globalified_lhs");
                }

                if (isa<GlobalVariable>(rhs)) {
                    rhs = builder.CreateCall(globalifyFunc, {rhs}, "globalified_rhs");
                }

                Value* newInstr = builder.CreateICmp(
                    static_cast<ICmpInst::Predicate>(constExpr->getPredicate()), 
                    lhs, 
                    rhs, 
                    "lowered_icmp"
                );

                instr.setOperand(operandIndex, newInstr);
            } else if (constExpr->getOpcode() == Instruction::GetElementPtr) {
                Value* gep_ptr = constExpr->getOperand(0);

                if (auto gv = dyn_cast<GlobalVariable>(gep_ptr)) {
                    builder.SetInsertPoint(&instr);

                    gep_ptr = builder.CreateCall(globalifyFunc, {gep_ptr}, "globalified_gep_ptr");
                    Type* gep_ptr_type = gv->getValueType();

                    std::vector<Value*> idxList;
                    for(uint64_t i = 1; i < constExpr->getNumOperands(); i++) {
                        idxList.push_back(constExpr->getOperand(i));
                    }

                    Value* newGepInstr = builder.CreateInBoundsGEP(gep_ptr_type, gep_ptr, idxList);

                    instr.setOperand(operandIndex, newGepInstr);
                }
            }
        } else if (type->isPointerTy()) {
            if (!isa<Constant>(operand)) {
                // operand is not const. do not globalify.
                continue;
            }

            auto *gv = dyn_cast<GlobalVariable>(operand);
            if (!gv || gv->getName().empty()) {
                // operand is not a global variable. do not globalify.
                continue;
            }

            // operand is a const global pointer. globalify it.

            oneConstGlobalified = true;
            builder.SetInsertPoint(&instr);

            Value *globalifyInvocationInstr = builder.CreateCall(
                globalifyFunc, {operand}, "globalified_ptr");

            // @Sun: this logic may not be correct long-term depending on how we implement
            // global/local addresses across function boundaries, especially regarding
            // library functions.
            if (instrOpcode == Instruction::Call || 
                instrOpcode == Instruction::CallBr || 
                instrOpcode == Instruction::Invoke) {

                if (operand->getType()->isPointerTy()) {
                    // we just deglobalize it for use inside the function

                    // @Sun: this is kinda gross and we should change this later.
                    
                    Value* deglobalifyInvocationInstr = builder.CreateCall(
                        deglobalifyFunc, 
                        {globalifyInvocationInstr}, 
                        "deglobalified_ptr"
                    );

                    instr.setOperand(operandIndex, deglobalifyInvocationInstr);
                } else {
                    // add a load which will get transformed by the load/store pass
                    LoadInst* load_instr = builder.CreateLoad(
                    type, globalifyInvocationInstr, "globalified_ptr_loaded");

                    instr.setOperand(operandIndex, load_instr);
                }
            } else {
                instr.setOperand(operandIndex, globalifyInvocationInstr);
            }
            
        }
    }
    return oneConstGlobalified;
}

static bool processBasicBlock(Module &m, IRBuilder<> &builder,
                              Function *globalifyFunc, Function *deglobalifyFunc, BasicBlock &bb) {
    bool oneConstGlobalified = false;
    for (Instruction &i : bb) {
        oneConstGlobalified |= processInstruction(m, builder, globalifyFunc, deglobalifyFunc, i);
    }
    return oneConstGlobalified;
}

PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam) {
    bool oneConstGlobalified = false;

    Function *globalifyFunc = m.getFunction("globalify");
    if (!globalifyFunc) {
        errs() << "[GLOBALIZE PASS] -- globalify function not found. exiting early.\n";
        return PreservedAnalyses::all();
    }

    Function *deglobalifyFunc = m.getFunction("deglobalify");
    if (!deglobalifyFunc) {
        errs() << "[GLOBALIZE PASS] -- deglobalify function not found. exiting early.\n";
        return PreservedAnalyses::all();
    }

    LLVMContext &context = m.getContext();
    IRBuilder<> builder(context);

    for (Function &f : m) {
        if (f.getName() == "__pando__replace_load_int64" ||
            f.getName() == "__pando__replace_load_int32" ||
            f.getName() == "__pando__replace_load_int16" ||
            f.getName() == "__pando__replace_load_int8" ||
            f.getName() == "__pando__replace_load_float32" ||
            f.getName() == "__pando__replace_load_float64" ||
            f.getName() == "__pando__replace_load_ptr" ||
            f.getName() == "__pando__replace_load_vector" ||
            f.getName() == "__pando__replace_store_int64" ||
            f.getName() == "__pando__replace_store_int32" ||
            f.getName() == "__pando__replace_store_int16" ||
            f.getName() == "__pando__replace_store_int8" ||
            f.getName() == "__pando__replace_store_float32" ||
            f.getName() == "__pando__replace_store_float64" ||
            f.getName() == "__pando__replace_store_ptr" ||
            f.getName() == "__pando__replace_store_vector" ||
            f.getName() == "check_if_global" || 
            f.getName() == "deglobalify" ||
            f.getName() == "globalify") {
            continue;
        }

        // process all instructions within this function
        for (BasicBlock &bb : f) {
            oneConstGlobalified |= processBasicBlock(m, builder, globalifyFunc, deglobalifyFunc, bb);
        }

        // if this is main(), globalify any type ptr parameters
        if(f.getName() == "main") {
            uint64_t numArgs = f.arg_size();
            for(uint64_t argIdx = 0; argIdx < numArgs; argIdx++) {
                Argument* arg = f.getArg(argIdx);
                if(arg->getType()->isPointerTy()) {
                    oneConstGlobalified = true;
                    builder.SetInsertPoint(f.getEntryBlock().begin());
                    Value *wrappedPtrArg = builder.CreateCall(
                        globalifyFunc, 
                        {arg}, 
                        "globalified_main_arg");

                    // this will replace the use within the call we just built...
                    arg->replaceAllUsesWith(wrappedPtrArg);

                    // ...so we now need to undo that.
                    Instruction* instr = f.getEntryBlock().getFirstNonPHI();
                    instr->setOperand(0, arg);
                }
            }
        }
    }

    return oneConstGlobalified ? PreservedAnalyses::none()
                            : PreservedAnalyses::all();
}

}; // struct GlobalizePass

} // end anonymous namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "GlobalizePass", LLVM_VERSION_STRING,
        [](PassBuilder &pb) {
            pb.registerPipelineParsingCallback(
                [](StringRef name, ModulePassManager &mpm,
                ArrayRef<PassBuilder::PipelineElement>) {
                    if (name == "globalize-pass") {
                        mpm.addPass(GlobalizePass());
                        return true;
                    } else if (name == "unwrap-lib-fn-call-params-pass") {
                        mpm.addPass(UnwrapLibFnCallParamsPass());
                        return true;
                    } else if (name == "globalize-global-unnamed-pointers-pass") {
                        mpm.addPass(GlobalizeGlobalUnnamedPointersPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

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

        if (type->isPointerTy()) {
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
            if (instrOpcode == Instruction::Call) {
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

        } else if (auto *constExpr = dyn_cast<ConstantExpr>(operand)) {
            // handle the constexpr-as-an-operand case. 
            // the constexpr can contain references to globals.

            // so far, we only care about ICmp instructions as constexpr operands.
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

                Value* new_instr = builder.CreateICmp(
                    static_cast<ICmpInst::Predicate>(constExpr->getPredicate()), 
                    lhs, 
                    rhs, 
                    "lowered_icmp"
                );

                instr.setOperand(operandIndex, new_instr);
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
    if (!globalifyFunc) {
        errs() << "[GLOBALIZE PASS] -- deglobalify function not found. exiting early.\n";
        return PreservedAnalyses::all();
    }

    IRBuilder<> builder(m.getContext());

    for (Function &f : m) {
        if (f.getName() == "__pando__replace_load64" ||
            f.getName() == "__pando__replace_load32" ||
            f.getName() == "__pando__replace_loadptr" ||
            f.getName() == "__pando__replace_store64" ||
            f.getName() == "__pando__replace_store32" ||
            f.getName() == "__pando__replace_storeptr" ||
            f.getName() == "check_if_global" || 
            f.getName() == "deglobalify" ||
            f.getName() == "globalify") {
            continue;
        }

        for (BasicBlock &bb : f) {
            oneConstGlobalified |= processBasicBlock(m, builder, globalifyFunc, deglobalifyFunc, bb);
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
                    }
                    return false;
                }
            );
        }
    };
}
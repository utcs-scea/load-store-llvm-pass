#include "llvm/IR/BasicBlock.h"
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
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <cstdint>

using namespace llvm;

namespace {

struct RemotifyGlobalPtrVarsPass
    : public PassInfoMixin<RemotifyGlobalPtrVarsPass> {

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
      errs() << "[GlobalizeGlobalUnnamedPointersPass] -- globalify function "
                "not found. exiting early.\n";
      return PreservedAnalyses::all();
    }

    LLVMContext &cx = m.getContext();

    FunctionType *ctorTy = FunctionType::get(Type::getVoidTy(cx), false);
    Function *ctor = Function::Create(ctorTy, Function::InternalLinkage,
                                      "global_ptr_transform_ctor", &m);

    BasicBlock *bb = BasicBlock::Create(cx, "entry", ctor);
    IRBuilder<> builder(bb);

    PointerType *int8PtrType = PointerType::get(Type::getInt8Ty(cx), 0);
    FunctionType *globalifyType =
        FunctionType::get(int8PtrType, {int8PtrType}, false);
    for (GlobalVariable &gv : m.globals()) {
      if (shouldGlobalizeVariable(gv)) {
        if (auto *gvPointee = dyn_cast<GlobalVariable>(gv.getInitializer())) {
          anyGlobalsModified = true;
          Value *gvPointeePtr = builder.CreateBitCast(gvPointee, int8PtrType);
          CallInst *globalifiedPointeePtr =
              builder.CreateCall(globalifyFunc, {gvPointeePtr});
          Value *castedPtr =
              builder.CreateBitCast(globalifiedPointeePtr, gv.getValueType());
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

struct WrapLibFnCallsPass : public PassInfoMixin<WrapLibFnCallsPass> {

  PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam) {
    bool anyOperandsAltered = false;

    Function *globalifyFunc = m.getFunction("globalify");
    if (!globalifyFunc) {
      errs() << "[WrapLibFnCallsPass] -- globalify function not found. exiting "
                "early.\n";
      return PreservedAnalyses::all();
    }

    Function *deglobalifyFunc = m.getFunction("deglobalify");
    if (!deglobalifyFunc) {
      errs() << "[WrapLibFnCallsPass] -- deglobalify function not found. "
                "exiting early.\n";
      return PreservedAnalyses::all();
    }

    Triple moduleTriple(m.getTargetTriple());
    TargetLibraryInfoImpl tlii(moduleTriple);
    TargetLibraryInfo tli(tlii);
    LibFunc inbuiltFunc;
    std::set<StringRef> builtins;
    for (Function &f : m) {
      if (tli.getLibFunc(f, inbuiltFunc)) {
        builtins.insert(f.getFunction().getName());
      }
    }

    LLVMContext &cx = m.getContext();
    IRBuilder<> builder(cx);

    uint64_t direct_calls = 0;
    uint64_t indirect_calls = 0;

    for (Function &f : m) {
      if (f.getName() == "__pando__replace_load_int64" ||
          f.getName() == "__pando__replace_load_int32" ||
          f.getName() == "__pando__replace_load_int16" ||
          f.getName() == "__pando__replace_load_int8" ||
          // f.getName() == "__pando__replace_load_int1" ||
          f.getName() == "__pando__replace_load_float32" ||
          f.getName() == "__pando__replace_load_float64" ||
          f.getName() == "__pando__replace_load_ptr" ||
          f.getName() == "__pando__replace_load_vector" ||
          f.getName() == "__pando__replace_store_int64" ||
          f.getName() == "__pando__replace_store_int32" ||
          f.getName() == "__pando__replace_store_int16" ||
          f.getName() == "__pando__replace_store_int8" ||
          // f.getName() == "__pando__replace_store_int1" ||
          f.getName() == "__pando__replace_store_float32" ||
          f.getName() == "__pando__replace_store_float64" ||
          f.getName() == "__pando__replace_store_ptr" ||
          f.getName() == "__pando__replace_store_vector" ||
          f.getName() == "check_if_global" || f.getName() == "deglobalify" ||
          f.getName() == "globalify") {
        continue;
      }

      // iterate over all instructions in the function
      for (BasicBlock &bb : f) {
        for (Instruction &instr : bb) {
          if (CallInst *callInstr = dyn_cast<CallInst>(&instr)) {

            Function *calledFunction = callInstr->getCalledFunction();
            if (calledFunction) {
              direct_calls++;
              StringRef funcName = calledFunction->getName();
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
            } else {
              // modify all indirect calls since we don't know what the function
              // is
              indirect_calls++;
            }

            // else, this is a call to a library function or intrinsic.

            // iterate over the operands...
            uint64_t numOperands = instr.getNumOperands();
            for (uint64_t operandIdx = 0; operandIdx < numOperands - 1;
                 operandIdx++) {
              Value *operand = instr.getOperand(operandIdx);
              if (operand->getType()->isPointerTy()) {
                builder.SetInsertPoint(&instr);
                Value *deglobalified_ptr = builder.CreateCall(
                    deglobalifyFunc, {operand}, "deglobalified_lib_ptr");
                instr.setOperand(operandIdx, deglobalified_ptr);
                anyOperandsAltered = true;
              }
            }

            // ...then check the function's return type...
            if (instr.getType()->isPointerTy()) {
              builder.SetInsertPoint(instr.getNextNode());
              Instruction *globalified_ptr =
                  builder.CreateCall(globalifyFunc, {dyn_cast<Value>(&instr)},
                                     "globalified_lib_ptr");
              instr.replaceAllUsesWith(globalified_ptr);

              // replaceAllUsesWith() will replace the pointer we just passed
              // into the globalify function call, so we now have to undo
              // that...
              globalified_ptr->setOperand(0, dyn_cast<Value>(&instr));
            }

          } else if (InvokeInst *invokeInstr = dyn_cast<InvokeInst>(&instr)) {

            Function *calledFunction = invokeInstr->getCalledFunction();
            if (calledFunction) {
              direct_calls++;
              StringRef funcName = calledFunction->getName();
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
            } else {
              // modify all indirect calls since we don't know what the function
              // is
              indirect_calls++;
            }

            // else, this is a call to a library function or intrinsic.

            // iterate over the operands...
            uint64_t numOperands = instr.getNumOperands();
            for (uint64_t operandIdx = 0; operandIdx < numOperands - 1; operandIdx++) {
              Value *operand = instr.getOperand(operandIdx);
              if (operand->getType()->isPointerTy()) {
                builder.SetInsertPoint(&instr);
                Value *deglobalified_ptr = builder.CreateCall(
                    deglobalifyFunc, {operand}, "deglobalified_lib_ptr");
                instr.setOperand(operandIdx, deglobalified_ptr);
                anyOperandsAltered = true;
              }
            }

            // // ...then check the function's return type...
            if (instr.getType()->isPointerTy()) {
              // we need to globalify this returned pointer, but an invoke instr
              // has to be the final instr in this bb. we have to instead write
              // the globalify calls in the basic blocks this invoke points to.

              BasicBlock *normalBb = invokeInstr->getNormalDest();
              Instruction *maybeNormalBbPhiInstr = normalBb->getFirstNonPHI()->getPrevNode();
              if (maybeNormalBbPhiInstr && maybeNormalBbPhiInstr->getOpcode() == Instruction::PHI) {
                // if BB has a phi instr, we must create a new intermediate
                // basic block for our globalify call, since no instrs can come
                // before the phi instr

                BasicBlock *intermediateBb = SplitEdge(instr.getParent(), normalBb);
                invokeInstr->setNormalDest(intermediateBb);

                builder.SetInsertPoint(intermediateBb->getFirstNonPHI());

                Instruction *globalified_ptr =
                    builder.CreateCall(globalifyFunc, {&instr},
                                       "globalified_lib_ptr_normal_intermedBb");

                // normalBb->removePredecessor(instr.getParent());
                // do we have to manually add intermediateBb as a pred...?

                for (PHINode &phi : normalBb->phis()) {
                  uint64_t numPhiEntries = phi.getNumIncomingValues();
                  for (uint64_t phiEntryIdx = 0; phiEntryIdx < numPhiEntries; phiEntryIdx++) {
                    if (phi.getIncomingBlock(phiEntryIdx) == intermediateBb &&
                        phi.getIncomingValueForBlock(intermediateBb) == dyn_cast<Value>(&instr)) {
                      // phi.removeIncomingValue(normalBb);
                      // phi.addIncoming(globalified_ptr, intermediateBb);
                      phi.setIncomingValue(phiEntryIdx, globalified_ptr);
                    }
                  }
                }

                instr.replaceAllUsesWith(globalified_ptr);
                globalified_ptr->setOperand(0, &instr);
              } else {
                // no phi node, so basic call insertion will work
                builder.SetInsertPoint(normalBb->getFirstNonPHI());
                Instruction *globalified_ptr =
                    builder.CreateCall(globalifyFunc, {dyn_cast<Value>(&instr)},
                                       "globalified_lib_ptr_normal_normalBb");
                instr.replaceAllUsesWith(globalified_ptr);

                // replaceAllUsesWith() will replace the pointer we just passed
                // into the globalify function call, so we now have to undo that...
                globalified_ptr->setOperand(0, &instr);
              }

              // BasicBlock *unwindBb = invokeInstr->getUnwindDest();
              // Instruction *maybeUnwindBbPhiInstr = normalBb->getFirstNonPHI()->getPrevNode();
              // the unwind basic block *may* contain a phi instr, then *must*
              // contain a landingpad instr.

              // the invoke's unwind block must be a landingpad block, so we
              // can't create an intermediate block.

              // this would get messy ...

              // for now, let's assert that we do not have any unwind blocks
              // which start with a phi instruction.
              // if(maybeUnwindBbPhiInstr) {
              //   assert(unwindBb->getFirstNonPHI()->getPrevNode()->getOpcode() != Instruction::PHI &&
              //         "we found an invoke unwind/landingpad block which has a phi node :(" &&
              //         "... gotta add this behavior.");
              // }

              // builder.SetInsertPoint(unwindBb->getFirstNonPHI()->getNextNode());
              // Instruction *globalified_ptr =
              //     builder.CreateCall(globalifyFunc, {dyn_cast<Value>(&instr)},
              //                        "globalified_lib_ptr_unwind");
              // instr.replaceAllUsesWith(globalified_ptr);

              // replaceAllUsesWith() will replace the pointer we just passed
              // into the globalify function call, so we now have to undo
              // that...
              // globalified_ptr->setOperand(0, dyn_cast<Value>(&instr));
            }
          }
        }
      }
    }

    // errs() << "direct calls: " << direct_calls << "\n";
    // errs() << "indirect calls: " << indirect_calls << "\n";

    return anyOperandsAltered ? PreservedAnalyses::none()
                              : PreservedAnalyses::all();
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

struct GlobalizePass : public PassInfoMixin<GlobalizePass> {

  static bool processInstruction(Module &m, IRBuilder<> &builder,
                                 Function *globalifyFunc,
                                 Function *deglobalifyFunc,
                                 Instruction &instr) {
    bool oneConstGlobalified = false;

    unsigned numOperands = instr.getNumOperands();
    uint64_t instrOpcode = instr.getOpcode();

    // iterate through the operands
    for (unsigned operandIndex = 0; operandIndex < numOperands;
         operandIndex++) {
      Value *operand = instr.getOperand(operandIndex);
      assert(operand);

      Type *type = operand->getType();
      assert(type);

      if (auto *constExpr = dyn_cast<ConstantExpr>(operand)) {
        // handle the constexpr-as-an-operand case.
        // the constexpr can contain references to globals.

        // so far, we only care about ICmp and GEP instructions as constexpr
        // operands. *there likely will be more.*
        if (constExpr->getOpcode() == Instruction::ICmp) {

          Value *lhs = constExpr->getOperand(0);
          Value *rhs = constExpr->getOperand(1);

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

          Value *newInstr = builder.CreateICmp(
              static_cast<ICmpInst::Predicate>(constExpr->getPredicate()), lhs,
              rhs, "lowered_icmp");

          instr.setOperand(operandIndex, newInstr);
        } else if (constExpr->getOpcode() == Instruction::GetElementPtr) {
          Value *gep_ptr = constExpr->getOperand(0);

          if (auto gv = dyn_cast<GlobalVariable>(gep_ptr)) {
            builder.SetInsertPoint(&instr);

            gep_ptr = builder.CreateCall(globalifyFunc, {gep_ptr},
                                         "globalified_gep_ptr");
            Type *gep_ptr_type = gv->getValueType();

            std::vector<Value *> idxList;
            for (uint64_t i = 1; i < constExpr->getNumOperands(); i++) {
              idxList.push_back(constExpr->getOperand(i));
            }

            Value *newGepInstr =
                builder.CreateInBoundsGEP(gep_ptr_type, gep_ptr, idxList);

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

        if (!gv->hasInitializer()) {
          continue;
        }

        // operand is a const global pointer. globalify it.

        oneConstGlobalified = true;
        builder.SetInsertPoint(&instr);

        Value *globalifyInvocationInstr =
            builder.CreateCall(globalifyFunc, {operand}, "globalified_ptr");

        instr.setOperand(operandIndex, globalifyInvocationInstr);
      }
    }
    return oneConstGlobalified;
  }

  static bool processBasicBlock(Module &m, IRBuilder<> &builder,
                                Function *globalifyFunc,
                                Function *deglobalifyFunc, BasicBlock &bb) {
    bool oneConstGlobalified = false;
    for (Instruction &i : bb) {
      oneConstGlobalified |=
          processInstruction(m, builder, globalifyFunc, deglobalifyFunc, i);
    }
    return oneConstGlobalified;
  }

  PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam) {
    bool oneConstGlobalified = false;

    Function *globalifyFunc = m.getFunction("globalify");
    if (!globalifyFunc) {
      errs() << "[GLOBALIZE PASS] -- globalify function not found. exiting "
                "early.\n";
      return PreservedAnalyses::all();
    }

    Function *deglobalifyFunc = m.getFunction("deglobalify");
    if (!deglobalifyFunc) {
      errs() << "[GLOBALIZE PASS] -- deglobalify function not found. exiting "
                "early.\n";
      return PreservedAnalyses::all();
    }

    LLVMContext &context = m.getContext();
    IRBuilder<> builder(context);

    for (Function &f : m) {
      if (f.getName() == "__pando__replace_load_int64" ||
          f.getName() == "__pando__replace_load_int32" ||
          f.getName() == "__pando__replace_load_int16" ||
          f.getName() == "__pando__replace_load_int8" ||
          // f.getName() == "__pando__replace_load_int1" ||
          f.getName() == "__pando__replace_load_float32" ||
          f.getName() == "__pando__replace_load_float64" ||
          f.getName() == "__pando__replace_load_ptr" ||
          f.getName() == "__pando__replace_load_vector" ||
          f.getName() == "__pando__replace_store_int64" ||
          f.getName() == "__pando__replace_store_int32" ||
          f.getName() == "__pando__replace_store_int16" ||
          f.getName() == "__pando__replace_store_int8" ||
          // f.getName() == "__pando__replace_store_int1" ||
          f.getName() == "__pando__replace_store_float32" ||
          f.getName() == "__pando__replace_store_float64" ||
          f.getName() == "__pando__replace_store_ptr" ||
          f.getName() == "__pando__replace_store_vector" ||
          f.getName() == "check_if_global" || f.getName() == "deglobalify" ||
          f.getName() == "globalify") {
        continue;
      }

      // process all instructions within this function
      for (BasicBlock &bb : f) {
        oneConstGlobalified |=
            processBasicBlock(m, builder, globalifyFunc, deglobalifyFunc, bb);
      }

      // if this is main(), globalify any type ptr parameters
      if (f.getName() == "main") {
        uint64_t numArgs = f.arg_size();
        for (uint64_t argIdx = 0; argIdx < numArgs; argIdx++) {
          Argument *arg = f.getArg(argIdx);
          if (arg->getType()->isPointerTy()) {
            oneConstGlobalified = true;
            builder.SetInsertPoint(f.getEntryBlock().begin());
            Value *wrappedPtrArg = builder.CreateCall(globalifyFunc, {arg},
                                                      "globalified_main_arg");

            // this will replace the use within the call we just built...
            arg->replaceAllUsesWith(wrappedPtrArg);

            // ...so we now need to undo that.
            Instruction *instr = f.getEntryBlock().getFirstNonPHI();
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

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "GlobalizePass", LLVM_VERSION_STRING,
          [](PassBuilder &pb) {
            pb.registerPipelineParsingCallback(
                [](StringRef name, ModulePassManager &mpm,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (name == "globalize-pass") {
                    mpm.addPass(GlobalizePass());
                    return true;
                  } else if (name == "wrap-lib-fn-calls-pass") {
                    mpm.addPass(WrapLibFnCallsPass());
                    return true;
                  } else if (name == "remotify-global-ptr-vars-pass") {
                    mpm.addPass(RemotifyGlobalPtrVarsPass());
                    return true;
                  }
                  return false;
                });
          }};
}
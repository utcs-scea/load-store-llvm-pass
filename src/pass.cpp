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
                                Function *globalifyFunc, Instruction &i) {
    bool oneConstGlobalified = false;
    unsigned numOperands = i.getNumOperands();
    // errs() << "\n(globalify-consts pass) -- new instr. num operands is "
        // << numOperands << ". instr is " << i << "\n";

    // iterate through the operands
    for (unsigned operandIndex = 0; operandIndex < numOperands; operandIndex++) {
        Value *operand = i.getOperand(operandIndex);
        assert(operand);

        Type *type = operand->getType();
        assert(type);

        // errs() << "\n  (globalify-consts pass) -- operand #" << operandIndex
            // << ", data is " << *operand << "\n";

        if (type->isPointerTy()) {

            // errs() << "\n  (globalify-consts pass) -- operand is of type pointer" << "\n";

            if (!isa<Constant>(operand)) {
                // operand is not const. do not globalify.
                continue;
            }

            // errs() << "\n  (globalify-consts pass) -- operand is const" << "\n";

            auto *gv = dyn_cast<GlobalVariable>(operand);
            if (!gv || gv->getName().empty()) {
                // operand is not a global variable. do not globalify.
                continue;
            }

            // TODO: REMOVE LATER
            if (gv->getName() != "hello") {
                continue;
            }

            // errs() << "  (globalify-consts pass) -- ptr + const + labelled. "
            //         "globalify it.\n";

            oneConstGlobalified = true;
            builder.SetInsertPoint(&i);

            // errs() << "  (globalify-consts pass) -- build "
            //         "globalify_invocation_instr\n";

            Value *globalifyInvocationInstr = builder.CreateCall(
                globalifyFunc, {operand}, "globalified_ptr");

            // errs() << "  (globalify-consts pass) -- set the operand\n";
            
            i.setOperand(operandIndex, globalifyInvocationInstr);

            // errs() << "  (globalify-consts pass) -- done!\n";

        } else if (auto *constExpr = dyn_cast<ConstantExpr>(operand)) {
            // handle the constexpr as an operand case here

            // errs() << "  (globalify-consts pass) -- operand is a ConstantExpr\n";

            if(constExpr->getOpcode() == Instruction::ICmp) {
                // errs() << "  (globalify-consts pass) -- operand's opcode is ICmp\n";

                Value* lhs = constExpr->getOperand(0);
                Value* rhs = constExpr->getOperand(1);

                if (!isa<GlobalVariable>(lhs) && !isa<GlobalVariable>(rhs)) {
                    continue;
                }

                builder.SetInsertPoint(&i);

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

                i.setOperand(operandIndex, new_instr);
            }
        } else if (auto *bbOperand = dyn_cast<BasicBlock>(operand)) {
            oneConstGlobalified |=
                processBasicBlock(m, builder, globalifyFunc, *bbOperand);
        }
    }
    return oneConstGlobalified;
}

static bool processBasicBlock(Module &m, IRBuilder<> &builder,
                                Function *globalifyFunc, BasicBlock &bb) {
    bool oneConstGlobalified = false;
    for (Instruction &i : bb) {
        oneConstGlobalified |= processInstruction(m, builder, globalifyFunc, i);
    }
    return oneConstGlobalified;
}

PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam) {
    bool oneConstGlobalified = false;
    Function *globalifyFunc = m.getFunction("globalify");
    if (!globalifyFunc) {
        return PreservedAnalyses::all();
    }

    IRBuilder<> builder(m.getContext());

    for (Function &f : m) {
        if (f.getName() == "__pando__replace_load64" ||
            f.getName() == "__pando__replace_loadptr" ||
            f.getName() == "__pando__replace_store64" ||
            f.getName() == "__pando__replace_storeptr" ||
            f.getName() == "check_if_global" || 
            f.getName() == "deglobalify" ||
            f.getName() == "globalify") {
            continue;
        }

        for (BasicBlock &bb : f) {
            oneConstGlobalified |= processBasicBlock(m, builder, globalifyFunc, bb);
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
#ifndef GLOBALIZE_PASS_H
#define GLOBALIZE_PASS_H

#include "llvm/Pass.h"

namespace llvm {
class GlobalizePass : public FunctionPass {
public:
  static char ID;
  GlobalizePass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;
};
}

#endif // GLOBALIZE_PASS_H
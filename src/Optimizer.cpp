#include "Optimizer.hpp"

#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

void runMem2Reg(llvm::Module &module) {
  llvm::PassBuilder passBuilder;

  llvm::LoopAnalysisManager loopAnalyses;
  llvm::FunctionAnalysisManager functionAnalyses;
  llvm::CGSCCAnalysisManager cgsccAnalyses;
  llvm::ModuleAnalysisManager moduleAnalyses;

  passBuilder.registerModuleAnalyses(moduleAnalyses);
  passBuilder.registerCGSCCAnalyses(cgsccAnalyses);
  passBuilder.registerFunctionAnalyses(functionAnalyses);
  passBuilder.registerLoopAnalyses(loopAnalyses);
  passBuilder.crossRegisterProxies(loopAnalyses, functionAnalyses, cgsccAnalyses,
                                   moduleAnalyses);

  llvm::FunctionPassManager functionPasses;
  functionPasses.addPass(llvm::PromotePass());

  for (llvm::Function &function : module) {
    if (function.isDeclaration())
      continue;
    functionPasses.run(function, functionAnalyses);
  }
}

#include "Optimizer.hpp"

#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

void run_mem2_reg(llvm::Module& module) {
    llvm::PassBuilder pass_builder;

    llvm::LoopAnalysisManager loop_analyses;
    llvm::FunctionAnalysisManager function_analyses;
    llvm::CGSCCAnalysisManager cgscc_analyses;
    llvm::ModuleAnalysisManager module_analyses;

    pass_builder.registerModuleAnalyses(module_analyses);
    pass_builder.registerCGSCCAnalyses(cgscc_analyses);
    pass_builder.registerFunctionAnalyses(function_analyses);
    pass_builder.registerLoopAnalyses(loop_analyses);
    pass_builder.crossRegisterProxies(loop_analyses, function_analyses, cgscc_analyses,
                                      module_analyses);

    llvm::FunctionPassManager function_passes;
    function_passes.addPass(llvm::PromotePass());

    for (llvm::Function& function : module) {
        if (function.isDeclaration())
            continue;
        function_passes.run(function, function_analyses);
    }
}

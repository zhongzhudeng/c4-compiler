#pragma once

namespace llvm {
class Module;
}

/// Runs LLVM's mem2reg pass, the only LLVM optimization permitted by the
/// assignment, so that our own SCCP operates on SSA form.
void run_mem2_reg(llvm::Module& module);

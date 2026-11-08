#pragma once

#include "llvm/IR/Module.h"

/// Sparse conditional constant propagation (Wegman & Zadeck) for the SSA form
/// produced by mem2reg.  The pass is implemented here and does not delegate to
/// any LLVM optimization pass.  It computes the constant/reachability lattice
/// to a fixed point and then rewrites the IR: constant operands are
/// substituted, constant conditional branches become unconditional, dead
/// instructions are removed and unreachable blocks are deleted.
class SCCP {
public:
  explicit SCCP(llvm::Module &module) : module_(module) {}

  void run();

private:
  llvm::Module &module_;
};

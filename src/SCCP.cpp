#include "SCCP.hpp"

#include "llvm/ADT/APInt.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"

#include <cassert>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using sccp::Constant;
using sccp::LatticeValue;
using sccp::Overdefined;
using sccp::Unknown;

llvm::ConstantInt *integerConstant(llvm::Type *type, const llvm::APInt &value) {
  return llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(type, value));
}

llvm::ConstantInt *foldBinary(unsigned opcode, llvm::ConstantInt *left,
                              llvm::ConstantInt *right) {
  llvm::APInt lhs = left->getValue();
  llvm::APInt rhs = right->getValue();
  llvm::Type *type = left->getType();
  unsigned bits = lhs.getBitWidth();

  switch (opcode) {
  case llvm::Instruction::Add:
    return integerConstant(type, lhs + rhs);
  case llvm::Instruction::Sub:
    return integerConstant(type, lhs - rhs);
  case llvm::Instruction::Mul:
    return integerConstant(type, lhs * rhs);
  case llvm::Instruction::UDiv:
    if (rhs.isZero())
      return nullptr;
    return integerConstant(type, lhs.udiv(rhs));
  case llvm::Instruction::SDiv:
    if (rhs.isZero())
      return nullptr;
    if (lhs.isMinSignedValue() && rhs.isAllOnes())
      return nullptr;
    return integerConstant(type, lhs.sdiv(rhs));
  case llvm::Instruction::URem:
    if (rhs.isZero())
      return nullptr;
    return integerConstant(type, lhs.urem(rhs));
  case llvm::Instruction::SRem:
    if (rhs.isZero())
      return nullptr;
    if (lhs.isMinSignedValue() && rhs.isAllOnes())
      return nullptr;
    return integerConstant(type, lhs.srem(rhs));
  case llvm::Instruction::And:
    return integerConstant(type, lhs & rhs);
  case llvm::Instruction::Or:
    return integerConstant(type, lhs | rhs);
  case llvm::Instruction::Xor:
    return integerConstant(type, lhs ^ rhs);
  case llvm::Instruction::Shl:
    if (rhs.uge(bits))
      return nullptr;
    return integerConstant(type, lhs.shl(rhs));
  case llvm::Instruction::LShr:
    if (rhs.uge(bits))
      return nullptr;
    return integerConstant(type, lhs.lshr(rhs));
  case llvm::Instruction::AShr:
    if (rhs.uge(bits))
      return nullptr;
    return integerConstant(type, lhs.ashr(rhs));
  default:
    return nullptr;
  }
}

llvm::ConstantInt *foldCompare(llvm::CmpInst::Predicate predicate,
                               llvm::ConstantInt *left,
                               llvm::ConstantInt *right,
                               llvm::LLVMContext &context) {
  llvm::APInt lhs = left->getValue();
  llvm::APInt rhs = right->getValue();
  bool result = false;

  switch (predicate) {
  case llvm::CmpInst::ICMP_EQ:
    result = lhs == rhs;
    break;
  case llvm::CmpInst::ICMP_NE:
    result = lhs != rhs;
    break;
  case llvm::CmpInst::ICMP_ULT:
    result = lhs.ult(rhs);
    break;
  case llvm::CmpInst::ICMP_ULE:
    result = lhs.ule(rhs);
    break;
  case llvm::CmpInst::ICMP_UGT:
    result = lhs.ugt(rhs);
    break;
  case llvm::CmpInst::ICMP_UGE:
    result = lhs.uge(rhs);
    break;
  case llvm::CmpInst::ICMP_SLT:
    result = lhs.slt(rhs);
    break;
  case llvm::CmpInst::ICMP_SLE:
    result = lhs.sle(rhs);
    break;
  case llvm::CmpInst::ICMP_SGT:
    result = lhs.sgt(rhs);
    break;
  case llvm::CmpInst::ICMP_SGE:
    result = lhs.sge(rhs);
    break;
  default:
    return nullptr;
  }

  return llvm::ConstantInt::getBool(context, result);
}

llvm::ConstantInt *foldCast(unsigned opcode, llvm::ConstantInt *operand,
                            llvm::Type *destination) {
  if (!destination->isIntegerTy())
    return nullptr;

  llvm::APInt value = operand->getValue();
  unsigned destinationBits = destination->getIntegerBitWidth();
  unsigned sourceBits = value.getBitWidth();

  switch (opcode) {
  case llvm::Instruction::Trunc:
    if (destinationBits >= sourceBits)
      return nullptr;
    return integerConstant(destination, value.trunc(destinationBits));
  case llvm::Instruction::ZExt:
    if (destinationBits <= sourceBits)
      return nullptr;
    return integerConstant(destination, value.zext(destinationBits));
  case llvm::Instruction::SExt:
    if (destinationBits <= sourceBits)
      return nullptr;
    return integerConstant(destination, value.sext(destinationBits));
  case llvm::Instruction::BitCast:
    if (destinationBits != sourceBits)
      return nullptr;
    return integerConstant(destination, value);
  default:
    return nullptr;
  }
}

bool hasEdge(const llvm::BasicBlock *predecessor,
             const llvm::BasicBlock *successor) {
  for (const llvm::BasicBlock *candidate : llvm::successors(predecessor))
    if (candidate == successor)
      return true;
  return false;
}

using Edge = std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *>;

/// Completed SCCP analysis for one function.  The pointers refer to the
/// function's instructions and blocks and are valid only until the
/// transformation starts deleting or replacing them.
struct AnalysisResult {
  std::unordered_map<const llvm::Instruction *, LatticeValue> values;
  std::unordered_set<const llvm::BasicBlock *> executableBlocks;
  std::set<Edge> executableEdges;

  LatticeValue stateOf(const llvm::Instruction &instruction) const {
    auto it = values.find(&instruction);
    if (it == values.end())
      return LatticeValue(Unknown{});
    return it->second;
  }

  bool isExecutable(const llvm::BasicBlock *block) const {
    return executableBlocks.count(block) != 0;
  }

  bool isExecutableEdge(const llvm::BasicBlock *from,
                        const llvm::BasicBlock *to) const {
    return executableEdges.count(Edge(from, to)) != 0;
  }
};

/// Computes the fixed point of the SCCP lattice for one function.  The
/// function is only read: all IR mutation happens in FunctionTransformer.
class FunctionAnalysis {
public:
  explicit FunctionAnalysis(const llvm::Function &function)
      : function_(function) {}

  AnalysisResult run() {
    // The entry block is executable by definition.  Executable blocks, edges
    // and lattice states only ever move up (see sccp::advance), so repeated
    // full sweeps terminate at a fixed point.
    result_.executableBlocks.insert(&function_.getEntryBlock());

    bool changed = true;
    while (changed) {
      changed = false;
      for (const llvm::BasicBlock &block : function_) {
        if (!result_.isExecutable(&block))
          continue;
        changed |= evaluateBlock(block);
        changed |= propagateTerminator(block);
      }
    }
    return std::move(result_);
  }

private:
  const llvm::Function &function_;
  AnalysisResult result_;

  LatticeValue stateOf(const llvm::Value *value) const {
    if (auto *constant = llvm::dyn_cast<llvm::ConstantInt>(value))
      return Constant(const_cast<llvm::ConstantInt *>(constant));
    if (auto *instruction = llvm::dyn_cast<llvm::Instruction>(value))
      return result_.stateOf(*instruction);
    // Arguments, globals, null pointers and other constants are opaque.
    return LatticeValue(Overdefined{});
  }

  LatticeValue evaluate(const llvm::Instruction &instruction) const {
    if (auto *binary = llvm::dyn_cast<llvm::BinaryOperator>(&instruction)) {
      LatticeValue left = stateOf(binary->getOperand(0));
      LatticeValue right = stateOf(binary->getOperand(1));
      if (auto state = sccp::propagatedState({&left, &right}))
        return *state;
      return sccp::constantOrOverdefined(
          foldBinary(binary->getOpcode(), sccp::constantValue(left),
                     sccp::constantValue(right)));
    }

    if (auto *compare = llvm::dyn_cast<llvm::ICmpInst>(&instruction)) {
      LatticeValue left = stateOf(compare->getOperand(0));
      LatticeValue right = stateOf(compare->getOperand(1));
      if (auto state = sccp::propagatedState({&left, &right}))
        return *state;
      return sccp::constantOrOverdefined(
          foldCompare(compare->getPredicate(), sccp::constantValue(left),
                      sccp::constantValue(right), function_.getContext()));
    }

    if (auto *cast = llvm::dyn_cast<llvm::CastInst>(&instruction)) {
      LatticeValue operand = stateOf(cast->getOperand(0));
      if (auto state = sccp::propagatedState({&operand}))
        return *state;
      return sccp::constantOrOverdefined(
          foldCast(cast->getOpcode(), sccp::constantValue(operand),
                   cast->getType()));
    }

    if (auto *select = llvm::dyn_cast<llvm::SelectInst>(&instruction)) {
      LatticeValue condition = stateOf(select->getCondition());
      LatticeValue whenTrue = stateOf(select->getTrueValue());
      LatticeValue whenFalse = stateOf(select->getFalseValue());

      if (sccp::isConstant(condition))
        return sccp::constantValue(condition)->isZero() ? whenFalse : whenTrue;

      // Equal constant arms yield that constant regardless of the condition.
      if (sccp::isConstant(whenTrue) && whenTrue == whenFalse)
        return whenTrue;

      if (sccp::isUnknown(condition))
        return LatticeValue(Unknown{});
      return LatticeValue(Overdefined{});
    }

    // Loads, calls, allocas, GEPs and anything else are opaque to this pass.
    return LatticeValue(Overdefined{});
  }

  LatticeValue evaluatePhi(const llvm::PHINode &phi) const {
    // Only incoming edges that the analysis has proven executable contribute;
    // a PHI may list predecessors that control flow never reaches.
    LatticeValue result = LatticeValue(Unknown{});
    for (unsigned i = 0; i < phi.getNumIncomingValues(); ++i) {
      const llvm::BasicBlock *predecessor = phi.getIncomingBlock(i);
      if (!result_.isExecutableEdge(predecessor, phi.getParent()))
        continue;
      result = sccp::join(result, stateOf(phi.getIncomingValue(i)));
    }
    return result;
  }

  bool evaluateBlock(const llvm::BasicBlock &block) {
    bool changed = false;
    for (const llvm::Instruction &instruction : block) {
      if (auto *phi = llvm::dyn_cast<llvm::PHINode>(&instruction)) {
        changed |=
            sccp::advance(result_.values[&instruction], evaluatePhi(*phi));
        continue;
      }

      if (instruction.isTerminator() || instruction.getType()->isVoidTy())
        continue;

      changed |=
          sccp::advance(result_.values[&instruction], evaluate(instruction));
    }
    return changed;
  }

  bool propagateTerminator(const llvm::BasicBlock &block) {
    // A constant condition makes exactly the taken edge executable, an
    // overdefined condition both edges, and an unknown condition none: the
    // edge is discovered once the condition's lattice state advances.
    const llvm::Instruction *terminator = block.getTerminator();

    if (auto *unconditional = llvm::dyn_cast<llvm::UncondBrInst>(terminator))
      return markEdge(&block, unconditional->getSuccessor(0));

    auto *conditional = llvm::dyn_cast<llvm::CondBrInst>(terminator);
    if (!conditional)
      return false;

    LatticeValue condition = stateOf(conditional->getCondition());
    if (sccp::isConstant(condition)) {
      bool taken = !sccp::constantValue(condition)->isZero();
      return markEdge(&block, conditional->getSuccessor(taken ? 0 : 1));
    }

    if (sccp::isOverdefined(condition)) {
      bool changed = false;
      for (unsigned i = 0; i < conditional->getNumSuccessors(); ++i)
        changed |= markEdge(&block, conditional->getSuccessor(i));
      return changed;
    }

    // An unknown condition does not make either edge executable yet.
    return false;
  }

  bool markEdge(const llvm::BasicBlock *from, const llvm::BasicBlock *to) {
    bool changed = false;
    if (result_.executableEdges.insert(Edge(from, to)).second)
      changed = true;
    if (result_.executableBlocks.insert(to).second)
      changed = true;
    return changed;
  }
};

/// Collapses duplicate PHI entries for a predecessor whose conditional branch
/// had both edges to the same successor.  After the branch becomes
/// unconditional only one predecessor edge remains, so only the value selected
/// by the taken edge may remain.
void deduplicatePhiEntries(llvm::BasicBlock &predecessor,
                           llvm::BasicBlock &successor, unsigned keptEdge) {
  for (llvm::PHINode &phi : successor.phis()) {
    std::vector<unsigned> occurrences;
    for (unsigned i = 0; i < phi.getNumIncomingValues(); ++i)
      if (phi.getIncomingBlock(i) == &predecessor)
        occurrences.push_back(i);
    if (occurrences.size() <= 1)
      continue;

    unsigned keep =
        occurrences[keptEdge < occurrences.size() ? keptEdge
                                                  : occurrences.size() - 1];
    llvm::Value *keptValue = phi.getIncomingValue(keep);
    for (auto it = occurrences.rbegin(); it != occurrences.rend(); ++it)
      phi.removeIncomingValue(*it, /*DeletePHIIfEmpty=*/false);
    phi.addIncoming(keptValue, &predecessor);
  }
}

/// Applies the transformations after the analysis has reached its fixed point.
/// The analysis result is consumed only by substituteConstants; the later
/// steps may delete the instructions and blocks it refers to, so they
/// recompute CFG facts from the rewritten IR.
class FunctionTransformer {
public:
  explicit FunctionTransformer(llvm::Function &function)
      : function_(function) {}

  void run(const AnalysisResult &analysis) {
    // substituteConstants is the only step that consumes the analysis result;
    // the steps below rewrite and delete IR, so they recompute CFG facts from
    // the rewritten function instead of trusting potentially stale pointers.
    substituteConstants(analysis);
    simplifyConditionalBranches();

    const BlockSet reachable = computeReachableBlocks();
    repairPhiInputs(reachable);
    deleteUnreachableBlocks(reachable);
    removeDeadInstructions();
  }

private:
  using BlockSet = std::unordered_set<const llvm::BasicBlock *>;

  llvm::Function &function_;

  void substituteConstants(const AnalysisResult &analysis) {
    for (const auto &[value, state] : analysis.values) {
      if (!sccp::isConstant(state))
        continue;
      auto *instruction = const_cast<llvm::Instruction *>(value);
      instruction->replaceAllUsesWith(sccp::constantValue(state));
    }
  }

  void simplifyConditionalBranches() {
    for (llvm::BasicBlock &block : function_) {
      auto *branch = llvm::dyn_cast<llvm::CondBrInst>(block.getTerminator());
      if (!branch)
        continue;

      auto *condition =
          llvm::dyn_cast<llvm::ConstantInt>(branch->getCondition());
      if (!condition)
        continue;

      unsigned takenEdge = condition->isZero() ? 1 : 0;
      llvm::BasicBlock *taken = branch->getSuccessor(takenEdge);
      if (branch->getSuccessor(0) == branch->getSuccessor(1))
        deduplicatePhiEntries(block, *taken, takenEdge);

      llvm::IRBuilder<> builder(&block, branch->getIterator());
      builder.CreateBr(taken);
      branch->eraseFromParent();
    }
  }

  BlockSet computeReachableBlocks() const {
    BlockSet reachable;
    std::vector<const llvm::BasicBlock *> worklist;
    reachable.insert(&function_.getEntryBlock());
    worklist.push_back(&function_.getEntryBlock());

    while (!worklist.empty()) {
      const llvm::BasicBlock *block = worklist.back();
      worklist.pop_back();
      const llvm::Instruction *terminator = block->getTerminator();
      for (unsigned i = 0; i < terminator->getNumSuccessors(); ++i) {
        const llvm::BasicBlock *successor = terminator->getSuccessor(i);
        if (reachable.insert(successor).second)
          worklist.push_back(successor);
      }
    }
    return reachable;
  }

  void repairPhiInputs(const BlockSet &reachable) {
    for (llvm::BasicBlock &block : function_) {
      if (!reachable.count(&block))
        continue;
      for (llvm::PHINode &phi : block.phis()) {
        phi.removeIncomingValueIf(
            [&](unsigned index) {
              const llvm::BasicBlock *incoming = phi.getIncomingBlock(index);
              return !reachable.count(incoming) || !hasEdge(incoming, &block);
            },
            /*DeletePHIIfEmpty=*/false);
        assert(phi.getNumIncomingValues() > 0 &&
               "reachable block with an empty PHI");
      }
    }
  }

  void deleteUnreachableBlocks(const BlockSet &reachable) {
    std::vector<llvm::BasicBlock *> dead;
    for (llvm::BasicBlock &block : function_)
      if (!reachable.count(&block))
        dead.push_back(&block);

    // Dead blocks may still refer to values defined in other dead blocks.
    // Replace their results with poison before dropping references so that
    // destroying one block cannot leave a dangling use-list entry in another.
    for (llvm::BasicBlock *block : dead)
      for (llvm::Instruction &instruction : *block)
        if (!instruction.getType()->isVoidTy() && !instruction.use_empty())
          instruction.replaceAllUsesWith(
              llvm::PoisonValue::get(instruction.getType()));

    for (llvm::BasicBlock *block : dead)
      block->dropAllReferences();
    for (llvm::BasicBlock *block : dead)
      block->eraseFromParent();
  }

  void removeDeadInstructions() {
    bool changed = true;
    while (changed) {
      changed = false;
      for (llvm::BasicBlock &block : function_) {
        for (auto it = block.begin(); it != block.end();) {
          llvm::Instruction *instruction = &*it++;
          if (llvm::isa<llvm::PHINode>(instruction) ||
              instruction->isTerminator())
            continue;
          if (instruction->use_empty() &&
              !instruction->mayHaveSideEffects()) {
            instruction->eraseFromParent();
            changed = true;
          }
        }
      }
    }
  }
};

/// One function's analysis followed by transformation.  Keeping the driver
/// separate from both stages makes the ordering explicit.
class FunctionSCCP {
public:
  explicit FunctionSCCP(llvm::Function &function) : function_(function) {}

  void run() {
    FunctionAnalysis analysis(function_);
    AnalysisResult result = analysis.run();
    FunctionTransformer transformer(function_);
    transformer.run(result);
  }

private:
  llvm::Function &function_;
};

} // namespace

void SCCP::run() {
  for (llvm::Function &function : module_) {
    if (function.isDeclaration())
      continue;
    FunctionSCCP pass(function);
    pass.run();
  }
}

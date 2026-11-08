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

/// One element of the SCCP lattice.  Undefined is the bottom element,
/// Overdefined the top element, and Constant a single known integer.
struct LatticeVal {
  enum class Kind { Undefined, Constant, Overdefined };

  Kind kind = Kind::Undefined;
  llvm::ConstantInt *constant = nullptr;

  static LatticeVal undefined() { return LatticeVal{}; }

  static LatticeVal overdefined() {
    LatticeVal value;
    value.kind = Kind::Overdefined;
    return value;
  }

  static LatticeVal constantValue(llvm::ConstantInt *constant) {
    LatticeVal value;
    value.kind = Kind::Constant;
    value.constant = constant;
    return value;
  }

  bool operator==(const LatticeVal &other) const {
    if (kind != other.kind)
      return false;
    if (kind != Kind::Constant)
      return true;
    if (constant == other.constant)
      return true;
    return constant->getBitWidth() == other.constant->getBitWidth() &&
           constant->getValue() == other.constant->getValue();
  }
};

LatticeVal meet(const LatticeVal &left, const LatticeVal &right) {
  if (left.kind == LatticeVal::Kind::Undefined)
    return right;
  if (right.kind == LatticeVal::Kind::Undefined)
    return left;
  if (left.kind == LatticeVal::Kind::Overdefined ||
      right.kind == LatticeVal::Kind::Overdefined)
    return LatticeVal::overdefined();
  if (left == right)
    return left;
  return LatticeVal::overdefined();
}

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

class FunctionSCCP {
public:
  explicit FunctionSCCP(llvm::Function &function) : function_(function) {}

  void run() {
    analyze();
    transform();
  }

private:
  using Edge = std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *>;

  llvm::Function &function_;
  std::unordered_map<const llvm::Value *, LatticeVal> values_;
  std::unordered_set<const llvm::BasicBlock *> executable_;
  std::set<Edge> edges_;

  LatticeVal latticeOf(const llvm::Value *value) const {
    if (auto *constant = llvm::dyn_cast<llvm::ConstantInt>(value))
      return LatticeVal::constantValue(
          const_cast<llvm::ConstantInt *>(constant));
    if (auto *instruction = llvm::dyn_cast<llvm::Instruction>(value)) {
      auto it = values_.find(instruction);
      if (it != values_.end())
        return it->second;
      return LatticeVal::undefined();
    }
    // Arguments, globals, null pointers and other constants are not tracked.
    return LatticeVal::overdefined();
  }

  LatticeVal evaluate(const llvm::Instruction &instruction) const {
    if (auto *binary = llvm::dyn_cast<llvm::BinaryOperator>(&instruction)) {
      LatticeVal left = latticeOf(binary->getOperand(0));
      LatticeVal right = latticeOf(binary->getOperand(1));
      if (left.kind == LatticeVal::Kind::Overdefined ||
          right.kind == LatticeVal::Kind::Overdefined)
        return LatticeVal::overdefined();
      if (left.kind == LatticeVal::Kind::Undefined ||
          right.kind == LatticeVal::Kind::Undefined)
        return LatticeVal::undefined();
      if (auto *folded =
              foldBinary(binary->getOpcode(), left.constant, right.constant))
        return LatticeVal::constantValue(folded);
      return LatticeVal::overdefined();
    }

    if (auto *compare = llvm::dyn_cast<llvm::ICmpInst>(&instruction)) {
      LatticeVal left = latticeOf(compare->getOperand(0));
      LatticeVal right = latticeOf(compare->getOperand(1));
      if (left.kind == LatticeVal::Kind::Overdefined ||
          right.kind == LatticeVal::Kind::Overdefined)
        return LatticeVal::overdefined();
      if (left.kind == LatticeVal::Kind::Undefined ||
          right.kind == LatticeVal::Kind::Undefined)
        return LatticeVal::undefined();
      if (auto *folded = foldCompare(compare->getPredicate(), left.constant,
                                     right.constant, function_.getContext()))
        return LatticeVal::constantValue(folded);
      return LatticeVal::overdefined();
    }

    if (auto *cast = llvm::dyn_cast<llvm::CastInst>(&instruction)) {
      LatticeVal operand = latticeOf(cast->getOperand(0));
      if (operand.kind == LatticeVal::Kind::Overdefined)
        return LatticeVal::overdefined();
      if (operand.kind == LatticeVal::Kind::Undefined)
        return LatticeVal::undefined();
      if (auto *folded =
              foldCast(cast->getOpcode(), operand.constant, cast->getType()))
        return LatticeVal::constantValue(folded);
      return LatticeVal::overdefined();
    }

    if (auto *select = llvm::dyn_cast<llvm::SelectInst>(&instruction)) {
      LatticeVal condition = latticeOf(select->getCondition());
      LatticeVal whenTrue = latticeOf(select->getTrueValue());
      LatticeVal whenFalse = latticeOf(select->getFalseValue());

      if (condition.kind == LatticeVal::Kind::Constant)
        return condition.constant->isZero() ? whenFalse : whenTrue;

      if (condition.kind == LatticeVal::Kind::Undefined) {
        if (whenTrue == whenFalse &&
            whenTrue.kind == LatticeVal::Kind::Constant)
          return whenTrue;
        return LatticeVal::undefined();
      }

      if (whenTrue == whenFalse &&
          whenTrue.kind == LatticeVal::Kind::Constant)
        return whenTrue;
      return LatticeVal::overdefined();
    }

    // Loads, calls, allocas, GEPs and anything else are opaque to this pass.
    return LatticeVal::overdefined();
  }

  LatticeVal evaluatePhi(const llvm::PHINode &phi) const {
    LatticeVal result = LatticeVal::undefined();
    for (unsigned i = 0; i < phi.getNumIncomingValues(); ++i) {
      const llvm::BasicBlock *predecessor = phi.getIncomingBlock(i);
      if (!edges_.count(Edge(predecessor, phi.getParent())))
        continue;
      result = meet(result, latticeOf(phi.getIncomingValue(i)));
    }
    return result;
  }

  bool markEdge(const llvm::BasicBlock *from, const llvm::BasicBlock *to) {
    bool changed = false;
    if (edges_.insert(Edge(from, to)).second)
      changed = true;
    if (executable_.insert(to).second)
      changed = true;
    return changed;
  }

  void analyze() {
    executable_.insert(&function_.getEntryBlock());

    bool changed = true;
    while (changed) {
      changed = false;

      for (llvm::BasicBlock &block : function_) {
        if (!executable_.count(&block))
          continue;

        for (llvm::Instruction &instruction : block) {
          if (auto *phi = llvm::dyn_cast<llvm::PHINode>(&instruction)) {
            LatticeVal value = evaluatePhi(*phi);
            auto it = values_.find(phi);
            if (it == values_.end() || !(it->second == value)) {
              values_[phi] = value;
              changed = true;
            }
            continue;
          }

          if (instruction.isTerminator() || instruction.getType()->isVoidTy())
            continue;

          LatticeVal value = evaluate(instruction);
          auto it = values_.find(&instruction);
          if (it == values_.end() || !(it->second == value)) {
            values_[&instruction] = value;
            changed = true;
          }
        }

        llvm::Instruction *terminator = block.getTerminator();
        if (auto *unconditional =
                llvm::dyn_cast<llvm::UncondBrInst>(terminator)) {
          changed |= markEdge(&block, unconditional->getSuccessor(0));
          continue;
        }

        auto *conditional = llvm::dyn_cast<llvm::CondBrInst>(terminator);
        if (!conditional)
          continue;

        LatticeVal condition = latticeOf(conditional->getCondition());
        if (condition.kind == LatticeVal::Kind::Constant) {
          bool taken = !condition.constant->isZero();
          changed |=
              markEdge(&block, conditional->getSuccessor(taken ? 0 : 1));
        } else if (condition.kind == LatticeVal::Kind::Overdefined) {
          for (unsigned i = 0; i < conditional->getNumSuccessors(); ++i)
            changed |= markEdge(&block, conditional->getSuccessor(i));
        }
      }
    }
  }

  void transform() {
    // 1. Replace every instruction whose lattice value is constant.
    for (const auto &[value, lattice] : values_) {
      if (lattice.kind != LatticeVal::Kind::Constant)
        continue;
      auto *instruction =
          llvm::cast<llvm::Instruction>(const_cast<llvm::Value *>(value));
      instruction->replaceAllUsesWith(lattice.constant);
    }

    // 2. Turn conditional branches with constant conditions into
    //    unconditional ones.
    for (llvm::BasicBlock &block : function_) {
      auto *branch = llvm::dyn_cast<llvm::CondBrInst>(block.getTerminator());
      if (!branch)
        continue;
      auto *condition =
          llvm::dyn_cast<llvm::ConstantInt>(branch->getCondition());
      if (!condition)
        continue;
      llvm::BasicBlock *taken =
          branch->getSuccessor(condition->isZero() ? 1 : 0);
      if (branch->getSuccessor(0) == branch->getSuccessor(1)) {
        // Both edges entered the same block.  Collapse the duplicate PHI
        // entries for this predecessor to the one selected by the edge that
        // the constant condition takes; after the branch becomes
        // unconditional there is only one predecessor edge.
        unsigned keptEdge = condition->isZero() ? 1 : 0;
        for (llvm::PHINode &phi : taken->phis()) {
          std::vector<unsigned> occurrences;
          for (unsigned i = 0; i < phi.getNumIncomingValues(); ++i)
            if (phi.getIncomingBlock(i) == &block)
              occurrences.push_back(i);
          if (occurrences.size() <= 1)
            continue;
          unsigned keep = occurrences[keptEdge < occurrences.size()
                                          ? keptEdge
                                          : occurrences.size() - 1];
          llvm::Value *keptValue = phi.getIncomingValue(keep);
          for (auto it = occurrences.rbegin(); it != occurrences.rend(); ++it)
            phi.removeIncomingValue(*it, /*DeletePHIIfEmpty=*/false);
          phi.addIncoming(keptValue, &block);
        }
      }
      llvm::IRBuilder<> builder(&block, branch->getIterator());
      builder.CreateBr(taken);
      branch->eraseFromParent();
    }

    // 3. Recompute reachability on the simplified CFG.
    std::unordered_set<const llvm::BasicBlock *> reachable;
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

    // 4. Drop PHI operands that come from unreachable blocks.
    for (llvm::BasicBlock &block : function_) {
      if (!reachable.count(&block))
        continue;
      for (llvm::PHINode &phi : block.phis()) {
        phi.removeIncomingValueIf(
            [&](unsigned i) {
              const llvm::BasicBlock *incoming = phi.getIncomingBlock(i);
              return !reachable.count(incoming) ||
                     !hasEdge(incoming, &block);
            },
            /*DeletePHIIfEmpty=*/false);
        assert(phi.getNumIncomingValues() > 0 &&
               "reachable block with an empty PHI");
      }
    }

    // 5. Delete unreachable blocks.  Replace their SSA results with poison
    //    first: dead blocks may still refer to values defined in other dead
    //    blocks, and destroying one block while another still uses its values
    //    would leave dangling operands in LLVM's use lists.
    std::vector<llvm::BasicBlock *> dead;
    for (llvm::BasicBlock &block : function_)
      if (!reachable.count(&block))
        dead.push_back(&block);

    for (llvm::BasicBlock *block : dead)
      for (llvm::Instruction &instruction : *block)
        if (!instruction.getType()->isVoidTy() && !instruction.use_empty())
          instruction.replaceAllUsesWith(
              llvm::PoisonValue::get(instruction.getType()));
    for (llvm::BasicBlock *block : dead)
      block->dropAllReferences();
    for (llvm::BasicBlock *block : dead)
      block->eraseFromParent();

    // 6. Remove instructions that became trivially dead.
    bool changed = true;
    while (changed) {
      changed = false;
      for (llvm::BasicBlock &block : function_) {
        for (auto it = block.begin(); it != block.end();) {
          llvm::Instruction *instruction = &*it++;
          if (llvm::isa<llvm::PHINode>(instruction) ||
              instruction->isTerminator())
            continue;
          if (instruction->use_empty() && !instruction->mayHaveSideEffects()) {
            instruction->eraseFromParent();
            changed = true;
          }
        }
      }
    }
  }
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

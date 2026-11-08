#pragma once

#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"

#include <cassert>
#include <initializer_list>
#include <optional>
#include <utility>
#include <variant>

/// The SCCP lattice and its operations.  The ordering is
///
///     Unknown < Constant(c) < Overdefined
///
/// Unknown is the bottom element ("no information yet"), Overdefined the top
/// element ("not a single known constant"), and distinct constants join to
/// Overdefined.
namespace sccp {

struct Unknown {
  bool operator==(const Unknown &) const = default;
};

struct Overdefined {
  bool operator==(const Overdefined &) const = default;
};

/// A constant lattice state.  The constructor asserts that the payload is
/// non-null, so a Constant alternative always carries a usable constant.
class Constant {
public:
  explicit Constant(llvm::ConstantInt *value) : value_(value) {
    assert(value_ && "Constant lattice state requires a constant");
  }

  llvm::ConstantInt *value() const { return value_; }

  /// Constants are compared by bit width and value, not by pointer identity.
  bool operator==(const Constant &other) const {
    if (value_ == other.value_)
      return true;
    return value_->getBitWidth() == other.value_->getBitWidth() &&
           value_->getValue() == other.value_->getValue();
  }

private:
  llvm::ConstantInt *value_;
};

using LatticeValue = std::variant<Unknown, Constant, Overdefined>;

inline bool isUnknown(const LatticeValue &state) {
  return std::holds_alternative<Unknown>(state);
}

inline bool isConstant(const LatticeValue &state) {
  return std::holds_alternative<Constant>(state);
}

inline bool isOverdefined(const LatticeValue &state) {
  return std::holds_alternative<Overdefined>(state);
}

inline llvm::ConstantInt *constantValue(const LatticeValue &state) {
  return std::get<Constant>(state).value();
}

/// Least upper bound.  Unknown is the identity, Overdefined dominates, equal
/// constants stay constant, and distinct constants join to Overdefined.
inline LatticeValue join(const LatticeValue &left, const LatticeValue &right) {
  if (isUnknown(left))
    return right;
  if (isUnknown(right))
    return left;
  if (isOverdefined(left) || isOverdefined(right))
    return Overdefined{};
  if (left == right)
    return left;
  return Overdefined{};
}

/// Moves the current state to join(current, next) and reports whether it
/// advanced.  Because the update is a join, an analysis state can never move
/// down the lattice, so every fixed-point iteration is monotone.
inline bool advance(LatticeValue &current, const LatticeValue &next) {
  LatticeValue joined = join(current, next);
  if (joined == current)
    return false;
  current = std::move(joined);
  return true;
}

/// The lattice result of an instruction-specific constant fold.  A null fold
/// means the operation is unsupported or undefined for these constants, so the
/// conservative result is Overdefined.
inline LatticeValue constantOrOverdefined(llvm::ConstantInt *value) {
  return value ? LatticeValue(Constant(value)) : LatticeValue(Overdefined{});
}

/// State for an operation that folds only when every operand is a known
/// constant.  Overdefined dominates Unknown; nullopt means every operand is a
/// Constant and instruction-specific folding can run.
inline std::optional<LatticeValue> propagatedState(
    std::initializer_list<const LatticeValue *> operands) {
  bool unknown = false;
  for (const LatticeValue *operand : operands) {
    if (isOverdefined(*operand))
      return LatticeValue(Overdefined{});
    if (isUnknown(*operand))
      unknown = true;
  }
  if (unknown)
    return LatticeValue(Unknown{});
  return std::nullopt;
}

} // namespace sccp

/// Sparse conditional constant propagation (Wegman & Zadeck) for the SSA form
/// produced by mem2reg.  The pass is implemented here and does not delegate to
/// any LLVM optimization pass.
///
/// For each function, FunctionSCCP first computes a complete analysis result
/// (lattice values, executable blocks and executable edges) and only then
/// rewrites the IR: constant operands are substituted, constant conditional
/// branches become unconditional, dead PHIs and blocks are repaired or
/// removed, and trivially dead instructions are deleted.
class SCCP {
public:
  explicit SCCP(llvm::Module &module) : module_(module) {}

  void run();

private:
  llvm::Module &module_;
};

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

llvm::ConstantInt* integer_constant(llvm::Type* type, const llvm::APInt& value) {
    return llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(type, value));
}

llvm::ConstantInt* fold_binary(unsigned opcode, llvm::ConstantInt* left, llvm::ConstantInt* right) {
    llvm::APInt lhs = left->getValue();
    llvm::APInt rhs = right->getValue();
    llvm::Type* type = left->getType();
    unsigned bits = lhs.getBitWidth();

    switch (opcode) {
        case llvm::Instruction::Add:
            return integer_constant(type, lhs + rhs);
        case llvm::Instruction::Sub:
            return integer_constant(type, lhs - rhs);
        case llvm::Instruction::Mul:
            return integer_constant(type, lhs * rhs);
        case llvm::Instruction::UDiv:
            if (rhs.isZero())
                return nullptr;
            return integer_constant(type, lhs.udiv(rhs));
        case llvm::Instruction::SDiv:
            if (rhs.isZero())
                return nullptr;
            if (lhs.isMinSignedValue() && rhs.isAllOnes())
                return nullptr;
            return integer_constant(type, lhs.sdiv(rhs));
        case llvm::Instruction::URem:
            if (rhs.isZero())
                return nullptr;
            return integer_constant(type, lhs.urem(rhs));
        case llvm::Instruction::SRem:
            if (rhs.isZero())
                return nullptr;
            if (lhs.isMinSignedValue() && rhs.isAllOnes())
                return nullptr;
            return integer_constant(type, lhs.srem(rhs));
        case llvm::Instruction::And:
            return integer_constant(type, lhs & rhs);
        case llvm::Instruction::Or:
            return integer_constant(type, lhs | rhs);
        case llvm::Instruction::Xor:
            return integer_constant(type, lhs ^ rhs);
        case llvm::Instruction::Shl:
            if (rhs.uge(bits))
                return nullptr;
            return integer_constant(type, lhs.shl(rhs));
        case llvm::Instruction::LShr:
            if (rhs.uge(bits))
                return nullptr;
            return integer_constant(type, lhs.lshr(rhs));
        case llvm::Instruction::AShr:
            if (rhs.uge(bits))
                return nullptr;
            return integer_constant(type, lhs.ashr(rhs));
        default:
            return nullptr;
    }
}

llvm::ConstantInt* fold_compare(llvm::CmpInst::Predicate predicate, llvm::ConstantInt* left,
                                llvm::ConstantInt* right, llvm::LLVMContext& context) {
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

llvm::ConstantInt* fold_cast(unsigned opcode, llvm::ConstantInt* operand, llvm::Type* destination) {
    if (!destination->isIntegerTy())
        return nullptr;

    llvm::APInt value = operand->getValue();
    unsigned destination_bits = destination->getIntegerBitWidth();
    unsigned source_bits = value.getBitWidth();

    switch (opcode) {
        case llvm::Instruction::Trunc:
            if (destination_bits >= source_bits)
                return nullptr;
            return integer_constant(destination, value.trunc(destination_bits));
        case llvm::Instruction::ZExt:
            if (destination_bits <= source_bits)
                return nullptr;
            return integer_constant(destination, value.zext(destination_bits));
        case llvm::Instruction::SExt:
            if (destination_bits <= source_bits)
                return nullptr;
            return integer_constant(destination, value.sext(destination_bits));
        case llvm::Instruction::BitCast:
            if (destination_bits != source_bits)
                return nullptr;
            return integer_constant(destination, value);
        default:
            return nullptr;
    }
}

bool has_edge(const llvm::BasicBlock* predecessor, const llvm::BasicBlock* successor) {
    for (const llvm::BasicBlock* candidate : llvm::successors(predecessor))
        if (candidate == successor)
            return true;
    return false;
}

using Edge = std::pair<const llvm::BasicBlock*, const llvm::BasicBlock*>;

/// Completed SCCP analysis for one function.  The pointers refer to the
/// function's instructions and blocks and are valid only until the
/// transformation starts deleting or replacing them.
struct AnalysisResult {
    std::unordered_map<const llvm::Instruction*, LatticeValue> values;
    std::unordered_set<const llvm::BasicBlock*> executable_blocks;
    std::set<Edge> executable_edges;

    LatticeValue state_of(const llvm::Instruction& instruction) const {
        auto it = values.find(&instruction);
        if (it == values.end())
            return LatticeValue(Unknown{});
        return it->second;
    }

    bool is_executable(const llvm::BasicBlock* block) const {
        return executable_blocks.count(block) != 0;
    }

    bool is_executable_edge(const llvm::BasicBlock* from, const llvm::BasicBlock* to) const {
        return executable_edges.count(Edge(from, to)) != 0;
    }
};

/// Computes the fixed point of the SCCP lattice for one function.  The
/// function is only read: all IR mutation happens in FunctionTransformer.
class FunctionAnalysis {
public:
    explicit FunctionAnalysis(const llvm::Function& function) : function_(function) {}

    AnalysisResult run() {
        // The entry block is executable by definition.  Executable blocks, edges
        // and lattice states only ever move up (see sccp::advance), so repeated
        // full sweeps terminate at a fixed point.
        result_.executable_blocks.insert(&function_.getEntryBlock());

        bool changed = true;
        while (changed) {
            changed = false;
            for (const llvm::BasicBlock& block : function_) {
                if (!result_.is_executable(&block))
                    continue;
                changed |= evaluate_block(block);
                changed |= propagate_terminator(block);
            }
        }
        return std::move(result_);
    }

private:
    const llvm::Function& function_;
    AnalysisResult result_;

    LatticeValue state_of(const llvm::Value* value) const {
        if (auto* constant = llvm::dyn_cast<llvm::ConstantInt>(value))
            return Constant(const_cast<llvm::ConstantInt*>(constant));
        if (auto* instruction = llvm::dyn_cast<llvm::Instruction>(value))
            return result_.state_of(*instruction);
        // Arguments, globals, null pointers and other constants are opaque.
        return LatticeValue(Overdefined{});
    }

    LatticeValue evaluate(const llvm::Instruction& instruction) const {
        if (auto* binary = llvm::dyn_cast<llvm::BinaryOperator>(&instruction)) {
            LatticeValue left = state_of(binary->getOperand(0));
            LatticeValue right = state_of(binary->getOperand(1));
            if (auto state = sccp::propagated_state({&left, &right}))
                return *state;
            return sccp::constant_or_overdefined(fold_binary(
                binary->getOpcode(), sccp::constant_value(left), sccp::constant_value(right)));
        }

        if (auto* compare = llvm::dyn_cast<llvm::ICmpInst>(&instruction)) {
            LatticeValue left = state_of(compare->getOperand(0));
            LatticeValue right = state_of(compare->getOperand(1));
            if (auto state = sccp::propagated_state({&left, &right}))
                return *state;
            return sccp::constant_or_overdefined(
                fold_compare(compare->getPredicate(), sccp::constant_value(left),
                             sccp::constant_value(right), function_.getContext()));
        }

        if (auto* cast = llvm::dyn_cast<llvm::CastInst>(&instruction)) {
            LatticeValue operand = state_of(cast->getOperand(0));
            if (auto state = sccp::propagated_state({&operand}))
                return *state;
            return sccp::constant_or_overdefined(
                fold_cast(cast->getOpcode(), sccp::constant_value(operand), cast->getType()));
        }

        if (auto* select = llvm::dyn_cast<llvm::SelectInst>(&instruction)) {
            LatticeValue condition = state_of(select->getCondition());
            LatticeValue when_true = state_of(select->getTrueValue());
            LatticeValue when_false = state_of(select->getFalseValue());

            if (sccp::is_constant(condition))
                return sccp::constant_value(condition)->isZero() ? when_false : when_true;

            // Equal constant arms yield that constant regardless of the condition.
            if (sccp::is_constant(when_true) && when_true == when_false)
                return when_true;

            if (sccp::is_unknown(condition))
                return LatticeValue(Unknown{});
            return LatticeValue(Overdefined{});
        }

        // Loads, calls, allocas, GEPs and anything else are opaque to this pass.
        return LatticeValue(Overdefined{});
    }

    LatticeValue evaluate_phi(const llvm::PHINode& phi) const {
        // Only incoming edges that the analysis has proven executable contribute;
        // a PHI may list predecessors that control flow never reaches.
        LatticeValue result = LatticeValue(Unknown{});
        for (unsigned i = 0; i < phi.getNumIncomingValues(); ++i) {
            const llvm::BasicBlock* predecessor = phi.getIncomingBlock(i);
            if (!result_.is_executable_edge(predecessor, phi.getParent()))
                continue;
            result = sccp::join(result, state_of(phi.getIncomingValue(i)));
        }
        return result;
    }

    bool evaluate_block(const llvm::BasicBlock& block) {
        bool changed = false;
        for (const llvm::Instruction& instruction : block) {
            if (auto* phi = llvm::dyn_cast<llvm::PHINode>(&instruction)) {
                changed |= sccp::advance(result_.values[&instruction], evaluate_phi(*phi));
                continue;
            }

            if (instruction.isTerminator() || instruction.getType()->isVoidTy())
                continue;

            changed |= sccp::advance(result_.values[&instruction], evaluate(instruction));
        }
        return changed;
    }

    bool propagate_terminator(const llvm::BasicBlock& block) {
        // A constant condition makes exactly the taken edge executable, an
        // overdefined condition both edges, and an unknown condition none: the
        // edge is discovered once the condition's lattice state advances.
        const llvm::Instruction* terminator = block.getTerminator();

        if (auto* unconditional = llvm::dyn_cast<llvm::UncondBrInst>(terminator))
            return mark_edge(&block, unconditional->getSuccessor(0));

        auto* conditional = llvm::dyn_cast<llvm::CondBrInst>(terminator);
        if (!conditional)
            return false;

        LatticeValue condition = state_of(conditional->getCondition());
        if (sccp::is_constant(condition)) {
            bool taken = !sccp::constant_value(condition)->isZero();
            return mark_edge(&block, conditional->getSuccessor(taken ? 0 : 1));
        }

        if (sccp::is_overdefined(condition)) {
            bool changed = false;
            for (unsigned i = 0; i < conditional->getNumSuccessors(); ++i)
                changed |= mark_edge(&block, conditional->getSuccessor(i));
            return changed;
        }

        // An unknown condition does not make either edge executable yet.
        return false;
    }

    bool mark_edge(const llvm::BasicBlock* from, const llvm::BasicBlock* to) {
        bool changed = false;
        if (result_.executable_edges.insert(Edge(from, to)).second)
            changed = true;
        if (result_.executable_blocks.insert(to).second)
            changed = true;
        return changed;
    }
};

/// Collapses duplicate PHI entries for a predecessor whose conditional branch
/// had both edges to the same successor.  After the branch becomes
/// unconditional only one predecessor edge remains, so only the value selected
/// by the taken edge may remain.
void deduplicate_phi_entries(llvm::BasicBlock& predecessor, llvm::BasicBlock& successor,
                             unsigned kept_edge) {
    for (llvm::PHINode& phi : successor.phis()) {
        std::vector<unsigned> occurrences;
        for (unsigned i = 0; i < phi.getNumIncomingValues(); ++i)
            if (phi.getIncomingBlock(i) == &predecessor)
                occurrences.push_back(i);
        if (occurrences.size() <= 1)
            continue;

        unsigned keep =
            occurrences[kept_edge < occurrences.size() ? kept_edge : occurrences.size() - 1];
        llvm::Value* kept_value = phi.getIncomingValue(keep);
        for (auto it = occurrences.rbegin(); it != occurrences.rend(); ++it)
            phi.removeIncomingValue(*it, /*DeletePHIIfEmpty=*/false);
        phi.addIncoming(kept_value, &predecessor);
    }
}

/// Applies the transformations after the analysis has reached its fixed point.
/// The analysis result is consumed only by substituteConstants; the later
/// steps may delete the instructions and blocks it refers to, so they
/// recompute CFG facts from the rewritten IR.
class FunctionTransformer {
public:
    explicit FunctionTransformer(llvm::Function& function) : function_(function) {}

    void run(const AnalysisResult& analysis) {
        // substituteConstants is the only step that consumes the analysis result;
        // the steps below rewrite and delete IR, so they recompute CFG facts from
        // the rewritten function instead of trusting potentially stale pointers.
        substitute_constants(analysis);
        simplify_conditional_branches();

        const BlockSet reachable = compute_reachable_blocks();
        repair_phi_inputs(reachable);
        delete_unreachable_blocks(reachable);
        remove_dead_instructions();
    }

private:
    using BlockSet = std::unordered_set<const llvm::BasicBlock*>;

    llvm::Function& function_;

    void substitute_constants(const AnalysisResult& analysis) {
        for (const auto& [value, state] : analysis.values) {
            if (!sccp::is_constant(state))
                continue;
            auto* instruction = const_cast<llvm::Instruction*>(value);
            instruction->replaceAllUsesWith(sccp::constant_value(state));
        }
    }

    void simplify_conditional_branches() {
        for (llvm::BasicBlock& block : function_) {
            auto* branch = llvm::dyn_cast<llvm::CondBrInst>(block.getTerminator());
            if (!branch)
                continue;

            auto* condition = llvm::dyn_cast<llvm::ConstantInt>(branch->getCondition());
            if (!condition)
                continue;

            unsigned taken_edge = condition->isZero() ? 1 : 0;
            llvm::BasicBlock* taken = branch->getSuccessor(taken_edge);
            if (branch->getSuccessor(0) == branch->getSuccessor(1))
                deduplicate_phi_entries(block, *taken, taken_edge);

            llvm::IRBuilder<> builder(&block, branch->getIterator());
            builder.CreateBr(taken);
            branch->eraseFromParent();
        }
    }

    BlockSet compute_reachable_blocks() const {
        BlockSet reachable;
        std::vector<const llvm::BasicBlock*> worklist;
        reachable.insert(&function_.getEntryBlock());
        worklist.push_back(&function_.getEntryBlock());

        while (!worklist.empty()) {
            const llvm::BasicBlock* block = worklist.back();
            worklist.pop_back();
            const llvm::Instruction* terminator = block->getTerminator();
            for (unsigned i = 0; i < terminator->getNumSuccessors(); ++i) {
                const llvm::BasicBlock* successor = terminator->getSuccessor(i);
                if (reachable.insert(successor).second)
                    worklist.push_back(successor);
            }
        }
        return reachable;
    }

    void repair_phi_inputs(const BlockSet& reachable) {
        for (llvm::BasicBlock& block : function_) {
            if (!reachable.count(&block))
                continue;
            for (llvm::PHINode& phi : block.phis()) {
                phi.removeIncomingValueIf(
                    [&](unsigned index) {
                        const llvm::BasicBlock* incoming = phi.getIncomingBlock(index);
                        return !reachable.count(incoming) || !has_edge(incoming, &block);
                    },
                    /*DeletePHIIfEmpty=*/false);
                assert(phi.getNumIncomingValues() > 0 && "reachable block with an empty PHI");
            }
        }
    }

    void delete_unreachable_blocks(const BlockSet& reachable) {
        std::vector<llvm::BasicBlock*> dead;
        for (llvm::BasicBlock& block : function_)
            if (!reachable.count(&block))
                dead.push_back(&block);

        // Dead blocks may still refer to values defined in other dead blocks.
        // Replace their results with poison before dropping references so that
        // destroying one block cannot leave a dangling use-list entry in another.
        for (llvm::BasicBlock* block : dead)
            for (llvm::Instruction& instruction : *block)
                if (!instruction.getType()->isVoidTy() && !instruction.use_empty())
                    instruction.replaceAllUsesWith(llvm::PoisonValue::get(instruction.getType()));

        for (llvm::BasicBlock* block : dead)
            block->dropAllReferences();
        for (llvm::BasicBlock* block : dead)
            block->eraseFromParent();
    }

    void remove_dead_instructions() {
        bool changed = true;
        while (changed) {
            changed = false;
            for (llvm::BasicBlock& block : function_) {
                for (auto it = block.begin(); it != block.end();) {
                    llvm::Instruction* instruction = &*it++;
                    if (llvm::isa<llvm::PHINode>(instruction) || instruction->isTerminator())
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

/// One function's analysis followed by transformation.  Keeping the driver
/// separate from both stages makes the ordering explicit.
class FunctionSCCP {
public:
    explicit FunctionSCCP(llvm::Function& function) : function_(function) {}

    void run() {
        FunctionAnalysis analysis(function_);
        AnalysisResult result = analysis.run();
        FunctionTransformer transformer(function_);
        transformer.run(result);
    }

private:
    llvm::Function& function_;
};

} // namespace

void SCCP::run() {
    for (llvm::Function& function : module_) {
        if (function.isDeclaration())
            continue;
        FunctionSCCP pass(function);
        pass.run();
    }
}

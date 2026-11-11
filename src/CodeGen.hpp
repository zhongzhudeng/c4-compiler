#pragma once

#include "AST.hpp"
#include "SemanticModel.hpp"
#include "Type.hpp"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

/// Lowers the typed AST to LLVM IR.  All types, declarations, value
/// categories and constant values come from the SemanticModel; this class only
/// translates them into instructions.
///
/// CodeGen owns the module-wide state: the LLVM module and context, the shared
/// TypeLayout and TypeContext, the EntityId maps for external objects and
/// functions, and module-wide string constants.  Each function definition is
/// lowered by a fresh FunctionEmitter, which owns the function, its return
/// type, local storage, label blocks, loop contexts and builders.  A
/// FunctionEmitter is constructed for exactly one definition and destroyed
/// afterwards, so there is no function state to reset between definitions.
class CodeGen {
public:
    CodeGen(const AST& ast, const SemanticModel& model, TypeContext& types, TypeLayout& layout,
            llvm::LLVMContext& context, llvm::Module& module);

    /// Generates IR for the whole translation unit and verifies the result.
    void emit();

private:
    class FunctionEmitter;

    const AST& ast_;
    const SemanticModel& model_;
    TypeContext& types_;
    TypeLayout& layout_;
    llvm::LLVMContext& context_;
    llvm::Module& module_;

    std::unordered_map<EntityId, llvm::GlobalVariable*> globals_;
    std::unordered_map<EntityId, llvm::Function*> functions_;
    unsigned string_counter_ = 0;

    void create_external_entities();
    llvm::Function* function_for(EntityId id);
    llvm::GlobalVariable* global_for(EntityId id);
    llvm::GlobalVariable* string_literal(const std::vector<char>& bytes);
};

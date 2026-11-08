#pragma once

#include "AST.hpp"
#include "SemanticModel.hpp"
#include "Support.hpp"
#include "Type.hpp"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <unordered_map>
#include <vector>

/// Lowers the typed AST to LLVM IR.  All types, declarations, value
/// categories and constant values come from the SemanticModel; this class only
/// translates them into instructions.  Every local object is materialized in
/// the entry block so that LLVM's mem2reg can promote it for --optimize.
class CodeGen {
public:
  CodeGen(const AST &ast, const SemanticModel &model, TypeLayout &layout,
          llvm::LLVMContext &context, llvm::Module &module,
          Diagnostics &diagnostics);

  /// Generates IR for the whole translation unit and verifies the result.
  void emit();

private:
  struct LoopContext {
    llvm::BasicBlock *breakBlock = nullptr;
    llvm::BasicBlock *continueBlock = nullptr;
  };

  const AST &ast_;
  const SemanticModel &model_;
  TypeLayout &layout_;
  llvm::LLVMContext &context_;
  llvm::Module &module_;
  Diagnostics &diagnostics_;

  llvm::IRBuilder<> builder_;
  llvm::IRBuilder<> allocaBuilder_;

  llvm::Function *function_ = nullptr;
  const Type *returnType_ = nullptr;
  std::unordered_map<uint32_t, llvm::AllocaInst *> locals_;
  std::unordered_map<Symbol, llvm::GlobalVariable *> globals_;
  std::unordered_map<Symbol, llvm::Function *> functions_;
  std::unordered_map<uint32_t, llvm::BasicBlock *> labels_;
  std::vector<LoopContext> loops_;
  unsigned stringCounter_ = 0;

  void createGlobalDecls();
  void createFunctionDecls();
  void emitFunction(DeclId id, const FunctionDecl &decl);
  void collectLabels(StmtId id);

  llvm::Function *functionFor(Symbol name, const Type *type);
  llvm::GlobalVariable *globalFor(Symbol name, const Type *type);
  llvm::Value *addressOfDecl(DeclId id);
  llvm::AllocaInst *createAlloca(const Type *type, Symbol name);

  void emitStmt(StmtId id);
  void emitCompound(StmtId id);
  void emitLabel(StmtId id, const LabelStmt &label);
  void emitLocalDecl(DeclId id);
  void ensureInsertionPoint();
  bool currentBlockTerminated() const;

  llvm::Value *emitExpr(ExprId id);
  llvm::Value *emitAddress(ExprId id);
  llvm::Value *emitInt(ExprId id);
  llvm::Value *emitLoad(const Type *type, llvm::Value *address);
  llvm::Value *emitStringLiteral(ExprId id, const StringLiteralExpr &string);
  llvm::Value *emitBinary(ExprId id, const BinaryExpr &binary);
  llvm::Value *emitLogical(ExprId id, const BinaryExpr &binary, bool isAnd);
  llvm::Value *emitConditional(ExprId id, const ConditionalExpr &conditional);
  llvm::Value *emitCall(ExprId id, const CallExpr &call);
  llvm::Value *emitPointerValue(ExprId id);
  llvm::Value *indexValue(ExprId id);
  llvm::Value *toBool(ExprId id);
  llvm::Value *coerce(llvm::Value *value, const Type *to, SourceLoc loc);

  static bool isPointerLike(const Type *type);
  static const Type *elementType(const Type *type);
};

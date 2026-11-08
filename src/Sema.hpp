#pragma once

#include "AST.hpp"
#include "SemanticModel.hpp"
#include "Support.hpp"
#include "Type.hpp"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

class Sema {
public:
  Sema(AST &ast, SemanticModel &model, TypeContext &types,
       TypeLayout &layout, Diagnostics &diagnostics);

  void analyze();

private:
  struct Scope {
    Scope *parent = nullptr;
    std::unordered_map<Symbol, DeclId> ordinary;
    std::unordered_map<Symbol, StructDecl *> tags;
  };

  struct DeclResult {
    const Type *type = nullptr;
    DeclaratorId functionDeclarator;
    std::vector<DeclId> params;
    Symbol name = nullptr;
    SourceLoc nameLoc;
  };

  struct FunctionContext {
    const Type *returnType = nullptr;
    int loopDepth = 0;
    std::unordered_map<Symbol, LabelId> labels;
    std::vector<std::pair<StmtId, Symbol>> pendingGotos;
  };

  AST &ast_;
  SemanticModel &model_;
  TypeContext &types_;
  TypeLayout &layout_;
  Diagnostics &diagnostics_;

  std::vector<std::unique_ptr<Scope>> scopes_;
  Scope *current_ = nullptr;
  FunctionContext *function_ = nullptr;

  // Functions have external linkage and therefore a single entity even when
  // they are first declared at block scope.  Objects at file scope are
  // tracked through the program scope's ordinary namespace.
  std::unordered_map<Symbol, DeclId> globalFunctions_;
  std::unordered_map<Symbol, DeclId> functionDefinitions_;

  void pushScope();
  void popScope();
  Scope *programScope() const;

  DeclId lookupOrdinary(Symbol name) const;
  StructDecl *lookupTag(Symbol name) const;
  DeclId declareGlobal(Symbol name, DeclId candidate, bool isFunction,
                       const Type *type, SourceLoc loc, bool isDefinition);
  void declareLocal(Symbol name, DeclId decl, SourceLoc loc);

  const Type *resolveTypeSpec(TypeSpecId id);
  DeclResult resolveDeclarator(DeclaratorId id, const Type *base);
  const Type *adjustParameterType(const Type *type, SourceLoc loc);
  void validateObjectType(const Type *type, SourceLoc loc,
                          const char *what) const;
  void validateFunctionType(const Type *type, SourceLoc loc) const;
  void validateFieldType(const Type *type, Symbol name, SourceLoc loc) const;
  bool isCompleteObject(const Type *type) const;

  void analyzeTopLevel(DeclId id);
  void analyzeLocalDecl(DeclId id);
  void analyzeLocalFunction(DeclId id, const DeclResult &result);
  void analyzeFunction(DeclId id);
  void analyzeCompound(StmtId id, bool pushScope);
  void analyzeStmt(StmtId id);

  ExprInfo &checkExpr(ExprId id);
  void checkScalar(ExprId id, SourceLoc loc, const char *what);
  const Type *decayedType(const ExprInfo &info);
  bool assignable(const Type *to, ExprId from);
  bool compatibleObjectPointers(const Type *a, const Type *b) const;
  bool compatibleEqualityPointers(const Type *a, const Type *b) const;
  void checkPointerArithmetic(const Type *pointer, SourceLoc loc) const;
  void setIntConstant(ExprInfo &info, long long value) const;
  void checkSizeofType(const Type *type, SourceLoc loc, ExprInfo &info) const;
};

#pragma once

#include "AST.hpp"
#include "SemanticModel.hpp"
#include "Support.hpp"
#include "Type.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

class Sema {
public:
  Sema(const AST &ast, SemanticModel &model, TypeContext &types,
       TypeLayout &layout, Diagnostics &diagnostics);

  void analyze();

private:
  struct DeclResult {
    const Type *type = nullptr;
    // Prototype scope of the function type that becomes the declared type.
    // A definition adopts this scope as its function scope.
    ScopeId functionScope;
    std::vector<DeclId> params;
    Symbol name;
    SourceLoc nameLoc;
  };

  struct FunctionContext {
    const Type *returnType = nullptr;
    int loopDepth = 0;
    ScopeId scope;
    std::vector<std::pair<StmtId, Symbol>> pendingGotos;
  };

  const AST &ast_;
  SemanticModel &model_;
  TypeContext &types_;
  TypeLayout &layout_;
  Diagnostics &diagnostics_;

  ScopeId current_;
  FunctionContext *function_ = nullptr;

  // Translation-unit index of external-linkage entities.  Lexical visibility
  // lives in scope bindings; this table implements the C rule that external
  // linkage determines entity identity even for a function first declared in a
  // block, where the file scope has no binding for it yet.
  std::unordered_map<Symbol, EntityId> externalEntities_;

  ScopeId pushScope(ScopeKind kind);
  void popScope();

  EntityId lookupOrdinary(Symbol name) const;
  StructDecl *lookupTag(Symbol name) const;

  EntityId declareFileObject(DeclId id, const DeclResult &result,
                             SourceLoc loc);
  EntityId declareFileFunction(DeclId id, const DeclResult &result,
                               SourceLoc loc, bool isDefinition);
  EntityId declareLocalObject(DeclId id, const DeclResult &result,
                              SourceLoc loc);
  EntityId declareLocalFunction(DeclId id, const DeclResult &result,
                                SourceLoc loc);
  void recordDeclaration(DeclId id, EntityId entity, ScopeId scope);

  const Type *resolveTypeSpec(TypeSpecId id, bool declareTag = false);
  DeclResult resolveDeclarator(DeclaratorId id, const Type *base);
  void adoptFunctionScope(ScopeId scope, ScopeId parent);
  const Type *adjustParameterType(const Type *type, SourceLoc loc);
  void validateObjectType(const Type *type, SourceLoc loc, const char *what,
                          bool requireComplete = true) const;
  void validateFunctionType(const Type *type, SourceLoc loc) const;
  void validateFieldType(const Type *type, Symbol name, SourceLoc loc) const;
  bool isCompleteObject(const Type *type) const;

  void analyzeTopLevel(DeclId id);
  void analyzeLocalDecl(DeclId id);
  void analyzeFunction(DeclId id, ScopeId functionScope);
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

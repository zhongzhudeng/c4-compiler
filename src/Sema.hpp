#pragma once
#include "Diagnostic.hpp"
#include "Scope.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "Token.hpp"
class Sema final {
  Sema(const Sema &) = delete;
  void operator=(const Sema &) = delete;

public:
  Sema();
  ~Sema();

  IdentifierExprPtr ActOnIdenExpr(Locatable loc, Symbol iden);
  ArraySubscriptExprPtr ActOnArraySubscriptExpr(Locatable loc, ExprPtr expr,
                                                ExprPtr squared_expr);

  DeclaratorPtr ActOnNonFuncDeclarator(DeclaratorPtr decl);
  FuncDefPtr ActOnFunctionDefinition(Locatable loc, DeclaratorDeclPtr decl,
                                     CompStmtPtr stmt);
  DeclaratorDeclPtr ActOnFunctionDecl(DeclaratorDeclPtr decl);

  StructDefPtr ActOnStructDefinition(Locatable loc, Token iden,
                                     std::vector<DeclaratorDeclPtr> fields);
  StructSpecPtr ActOnStructSpec(Locatable loc, Symbol iden);
  StructSpecPtr ActOnStructSpec(Locatable loc, StructDefPtr struct_def);

  DeclaratorDeclPtr ActOnDeclaration(Locatable loc, DeclSpecPtr spec,
                                     DeclaratorPtr decl);

  LabeledStmtPtr ActOnLabeledStmt(Locatable loc, LabelDeclPtr label_decl,
                                  StmtPtr stmt);
  GoToStmtPtr ActOnGotoStmt(Locatable loc, Symbol iden);
  void resolve_backing_label_decl();
  void enter_func_scope();
  void exit_func_scope();
  void enter_scope();
  void exit_scope();
  void add_parameters(const FunctionDeclarator &function_declarator);

private:
  Scope *cur_scope = nullptr;
  FuncScope *cur_func_scope = nullptr;
  ProgramScope *program_scope = nullptr;
};
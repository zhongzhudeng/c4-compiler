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
  StructDefPtr ActOnStructDefinition(Locatable loc, Token iden,
                                     std::vector<DeclPtr> fields);
  StructSpecPtr ActOnStructSpec(Locatable loc, Symbol iden);
  StructSpecPtr ActOnStructSpec(Locatable loc, StructDefPtr struct_def);

  DeclaratorDeclPtr ActOnDeclaration(Locatable loc, DeclSpecPtr spec,
                                     DeclaratorPtr decl);

  FuncDefPtr ActOnFunctionDefinition(Locatable loc, DeclaratorDeclPtr decl,
                                     CompStmtPtr compoundStmt);

  LabeledStmtPtr ActOnLabeledStmt(Locatable loc, LabelDeclPtr label_decl,
                                  StmtPtr stmt);
  GoToStmtPtr ActOnGotoStmt(Locatable loc, Symbol iden);
  void resolve_backing_label_decl();
  void enter_func_scope();
  void exit_func_scope();

private:
  Scope *cur_scope;
  FuncScope *cur_func_scope;
  ProgramScope *program_scope;
};
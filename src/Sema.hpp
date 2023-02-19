#pragma once
#include "Scope.hpp"
#include "Syntax.hpp"
#include "Token.hpp"
class Sema final {
  Sema(const Sema &) = delete;
  void operator=(const Sema &) = delete;

public:
  Sema();
  ~Sema();
  LabeledStmtPtr ActOnLabeledStmt(Locatable loc, LabelDeclPtr label_decl,
                                      StmtPtr stmt);
  GoToStmtPtr ActOnGotoStmt(Locatable loc, Symbol iden);
  void resolve_backing_label_decl();

private:
  FuncScope *cur_func_scope;
};
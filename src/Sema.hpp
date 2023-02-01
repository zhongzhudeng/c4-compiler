#pragma once
#include "Syntax.hpp"
#include "Token.hpp"
class Sema final {
  Sema(const Sema &) = delete;
  void operator=(const Sema &) = delete;

public:
  Sema();
  ~Sema();
  LabeledStmtPtr ActOnLabeledStmt(Token idenTok, StmtPtr stmt);
  GoToStmtPtr ActOnGotoStmt(Locatable loc, Token idenTok);
};
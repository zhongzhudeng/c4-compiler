#include "Sema.hpp"
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxStmt.hpp"
#include "Token.hpp"

Sema::Sema() {}
Sema::~Sema() {}

LabeledStmtPtr Sema::ActOnLabeledStmt(Token idenTok, StmtPtr stmt) {
  return std::make_unique<LabeledStmt>(idenTok, idenTok.Text, std::move(stmt));
}

GoToStmtPtr Sema::ActOnGotoStmt(Locatable loc, Token idenTok){
  return std::make_unique<GoToStmt>(loc, idenTok.Text);
}
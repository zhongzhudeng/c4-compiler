#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxStmt.hpp"
#include "Token.hpp"
#include <memory>
#include <vector>

#include "FirstFollow.def"

StmtPtr Parser::ParseStmt() {
  Locatable loc(location());
  switch (kind()) {
  default:
    errorloc(loc, "Unexpected Token '", *cursor->Text,
             "' when expecting statement");
  case TK_LPAREN: {
    ExprPtr expr = ParseExpr();
    expect(TK_SEMICOLON, "';' after expression in statement");
    return std::make_unique<ExprStmt>(loc, std::move(expr));
  }

  case TK_IDENTIFIER: {
    Symbol iden = cursor->Text;
    Token IdenTok = *cursor;
    switch (kind_first()) {
    default: {
      ExprPtr expr = ParseExpr();
      expect(TK_SEMICOLON, "';' after expression in statement");
      return std::make_unique<ExprStmt>(loc, std::move(expr));
    }
    case TK_COLON:
      bump(), bump();
      LabelDeclPtr label_decl = std::make_unique<LabelDecl>(loc, iden);
      StmtPtr stmt = ParseStmt();
      return Action.ActOnLabeledStmt(loc, std::move(label_decl),
                                     std::move(stmt));
    }
  }
  case TK_LBRACE: {
    Action.enter_scope();
    auto stmt = ParseCompoundStmt();
    Action.exit_scope();
    return stmt;
  }
  case TK_IF: {
    bump();
    expect(TK_LPAREN, "'('");
    ExprPtr cond = ParseExpr();
    expect(TK_RPAREN, "')'");
    StmtPtr then = ParseStmt();
    StmtPtr Else = nullptr;
    if (accept(TK_ELSE))
      Else = ParseStmt();
    return std::make_unique<IfElseStmt>(loc, std::move(cond), std::move(then),
                                        std::move(Else));
  }
  case TK_WHILE: {
    bump();
    expect(TK_LPAREN, "'(' after while");
    ExprPtr expr = ParseExpr();
    expect(TK_RPAREN, "')' after expression in while statement");
    StmtPtr stmt = ParseStmt();
    return std::make_unique<WhileStmt>(loc, std::move(expr), std::move(stmt));
  }
  case TK_GOTO: {
    bump();
    Symbol iden = cursor->Text;
    expect(TK_IDENTIFIER, "identifier after goto");
    expect(TK_SEMICOLON, "';'");
    return Action.ActOnGotoStmt(loc, iden);
  }
  case TK_CONTINUE: {
    bump();
    StmtPtr stmt = std::make_unique<ContinueStmt>(loc);
    expect(TK_SEMICOLON, "';'");
    return stmt;
  }
  case TK_BREAK: {
    bump();
    StmtPtr stmt = std::make_unique<BreakStmt>(loc);
    expect(TK_SEMICOLON, "';'");
    return stmt;
  }
  case TK_RETURN: {
    bump();
    ExprPtr expr = nullptr;
    if (kind() != TK_SEMICOLON)
      expr = ParseExpr();
    StmtPtr stmt = std::make_unique<ReturnStmt>(loc, std::move(expr));
    expect(TK_SEMICOLON, "';'");
    return stmt;
  }
  case TK_SEMICOLON:
    bump();
    return std::make_unique<ExprStmt>(loc, nullptr);
  }
}

CompStmtPtr Parser::ParseCompoundStmt() {
  Locatable loc(location());
  expect(TK_LBRACE, "'{' in compound statement");
  std::vector<StmtPtr> bil;
  while (!accept(TK_RBRACE)) {
    switch (kind()) {
    default:
      bil.push_back(ParseStmt());
      break;
      DECLARATION
      Locatable l(location());
      bil.push_back(std::make_unique<DeclStmt>(l, ParseDecl()));
      expect(TK_SEMICOLON, "';' after declaration");
      break;
    }
  }
  return std::make_unique<CompStmt>(loc, std::move(bil));
}
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

StmtPtr Parser::parseStmt() {
  Locatable loc(location());
  switch (kind()) {
  default:
    errorloc(loc, "Unexpected Token '", *cursor->Text,
             "' when expecting statement");
  case TK_IDENTIFIER: {

    Symbol iden = cursor->Text;
    switch (kind_first()) {
    default: {
      auto expr = parseExpr();
      expect(TK_SEMICOLON, "';' after expression in statement");
      return std::make_unique<ExprStmt>(loc, std::move(expr));
    }
    case TK_COLON:
      bump(), bump();
      StmtPtr stmt = parseStmt();
      return std::make_unique<IdentifierStmt>(loc, iden, parseStmt());
    }
  }
  case TK_LBRACE: {
    return parseCompoundStmt();
  }
  case TK_IF: {
    bump();
    expect(TK_LPAREN, "'('");
    auto expr = parseExpr();
    expect(TK_RPAREN, "')'");
    auto stmt = parseStmt();
    if (accept(TK_ELSE)) {
      auto elseStmt = parseStmt();
      return std::make_unique<IfElseStmt>(loc, std::move(expr), std::move(stmt),
                                          std::move(elseStmt));
    } else
      return std::make_unique<IfStmt>(loc, std::move(expr), std::move(stmt));
  }
  case TK_WHILE: {
    bump();
    expect(TK_LPAREN, "'(' after while");
    ExprPtr expr = parseExpr();
    expect(TK_RPAREN, "')' after expression in while statement");
    StmtPtr stmt = parseStmt();
    return std::make_unique<WhileStmt>(loc, std::move(expr), std::move(stmt));
  }
  case TK_GOTO: {
    bump();
    Symbol iden = cursor->Text;
    expect(TK_IDENTIFIER, "identifier after goto");
    expect(TK_SEMICOLON, "';'");
    return std::make_unique<GoToStmt>(loc, iden);
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
      expr = parseExpr();
    StmtPtr stmt = std::make_unique<ReturnStmt>(loc, std::move(expr));
    expect(TK_SEMICOLON, "';'");
    return stmt;
  }
  case TK_SEMICOLON:
    bump();
    return std::make_unique<ExprStmt>(loc, nullptr);
  }
}

CompStmtPtr Parser::parseCompoundStmt() {
  Locatable loc(location());
  expect(TK_LBRACE, "'{' in compound statement");
  std::vector<StmtPtr> bil;
  while (!accept(TK_RBRACE)) {
    switch (kind()) {
    default:
      bil.push_back(parseStmt());
      break;
      DECLARATION
      Locatable l(location());
      bil.push_back(std::make_unique<DeclStmt>(l, parseDecl()));
      expect(TK_SEMICOLON, "';' after declaration");
      break;
    }
  }
  return std::make_unique<CompStmt>(loc, std::move(bil));
}
#include "ParserImpl.hpp"
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"
#include "Token.hpp"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

StmtPtr Parser::impl::parseStmt(){
  StmtPtr stmt;
  switch (cursor->Kind) {
  default:{
    auto expr = parseExpr();
    stmt = std::make_unique<ExprStmt>(std::move(expr));
    break;}
  case TK_IDENTIFIER:{
    if(kind_first() == TK_COLON){
      Symbol identifier = cursor->Text;
      bump();
      bump();
      StmtPtr RHS = parseStmt();
      stmt = std::make_unique<IdentifierStmt>(identifier, std::move(stmt));
    } else {
      auto identifier = parseIdentifierExpr();
      stmt = std::make_unique<ExprStmt>(std::move(identifier));
      expect(TK_SEMICOLON,"';'");
    }
    break;}    
  case (TK_LBRACE):{
    bump();
    std::vector<StmtPtr> bil;
    if (!check(TK_RBRACE)){
      bil = parseBlockItemList();
    }
    expect(TK_RBRACE, "'}'");
    stmt = std::make_unique<CompStmt>(std::move(bil));
    break;}
  case (TK_IF):{
    bump();
    expect(TK_LPAREN,"'('");
    auto expr = parseExpr();
    expect(TK_RPAREN,"')'");
    auto ifStmt = parseStmt();
    if(check(TK_ELSE)){
      bump();
      auto elseStmt = parseStmt();
      stmt = std::make_unique<IfElseStmt>(std::move(expr),std::move(ifStmt),std::move(elseStmt));
    } else {
      stmt = std::make_unique<IfStmt>(std::move(expr),std::move(ifStmt));
    }
    break;}
  case (TK_WHILE):{
    bump();
    expect(TK_LPAREN,"'('");
    auto expr = parseExpr();
    expect(TK_RPAREN,"')'");
    auto doStmt = parseStmt();
    stmt = std::make_unique<WhileStmt>(std::move(expr),std::move(doStmt));
    break;}
  case TK_GOTO:{
    bump();
    if (check(TK_IDENTIFIER)){
      Symbol identifier = cursor->Text;
      bump();
      stmt = std::make_unique<GoToStmt>(identifier);
    } else {
      errorloc(location(), "Unknown token ", cursor->Text,
         " when expecting identifier\n");
    }
    break;}
  case TK_CONTINUE:{
    stmt = std::make_unique<ContinueStmt>();
    expect(TK_SEMICOLON,"';'");
    break;}
  case TK_BREAK:{
    stmt = std::make_unique<BreakStmt>();
    expect(TK_SEMICOLON,"';'");
    break;}
  case TK_SEMICOLON:{
    bump();
    // EMPTY EXPRESSION??
    auto expr = std::make_unique<Expr>(location());
    stmt = std::make_unique<ExprStmt>(std::move(expr));
    break;}
  }
  return stmt;
};
std::vector<StmtPtr> Parser::impl::parseBlockItemList(){
  std::vector<StmtPtr> bil;
  // TODO parse Statement or Declaration
  return bil;
};

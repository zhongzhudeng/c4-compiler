#pragma once
#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "Sema.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "Token.hpp"
#include <functional>
#include <vector>

class UnitTestParser;

class Parser {
public:
  Parser(std::vector<Token> token_stream, Sema &Action);
  ~Parser();
  TranslationUnitPtr parse();

private:
  std::vector<Token> token_stream;
  std::vector<Token>::iterator cursor;
  std::vector<Token>::const_iterator end;

  Sema &Action;

  friend UnitTestParser;

  const Locatable &location(void);
  void bump(void);
  bool check(TokenKind tk);
  bool accept(TokenKind tk);
  void expect(TokenKind tk, const char *txt);
  TokenKind kind();
  TokenKind kind_first();
  bool is_paren_type_name();
  bool is_end();

  ExprPtr ParseExpr();

  DeclSpecPtr ParseDeclSpec();
  DeclaratorDeclPtr ParseDeclaratorDecl();
  DeclaratorPtr ParseDeclarator();
  DeclaratorPtr ParseNonFuncDeclarator();

  StmtPtr ParseStmt();

  ExprPtr ParseUnaryExpr();
  ExprPtr ParseBinaryExprRHS(int, ExprPtr);
  ExprPtr ParsePostfixExprSuffix(ExprPtr expr);

  ExprPtr ParseIdentifierExpr();
  ExprPtr ParseConstantExpr();
  ExprPtr ParseStringLiteralExpr();
  ExprPtr ParseParenExpr();

  CompStmtPtr ParseCompoundStmt();
  DeclPtr ParseExternDecl();
};
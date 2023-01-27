#pragma once
#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "Syntax.hpp"
#include "Token.hpp"
#include <functional>
#include <vector>

template <typename AST> class UnitTestParser;

class Parser {
public:
  Parser(std::vector<Token> token_stream);
  void parse(std::function<void(ExternDeclPtr, bool)> f);

private:
  std::vector<Token> token_stream;
  std::vector<Token>::iterator cursor;
  std::vector<Token>::const_iterator end;
  friend UnitTestParser<Expr>;
  friend UnitTestParser<Decl>;
  friend UnitTestParser<Stmt>;
  friend UnitTestParser<ExternDecl>;

  const Locatable &location(void);
  void bump(void);
  bool check(TokenKind tk);
  bool accept(TokenKind tk);
  void expect(TokenKind tk, const char *txt);
  TokenKind kind();
  TokenKind kind_first();
  bool is_paren_type_name();
  bool is_end();

  ExprPtr parseExpr();
  DeclPtr parseDecl(DeclKind dKind = ANY);
  StmtPtr parseStmt();

  ExprPtr parseUnaryExpr();
  ExprPtr parseBinaryExprRHS(int, ExprPtr);
  ExprPtr parsePostfixExprSuffix(ExprPtr expr);

  ExprPtr parseIdentifierExpr();
  ExprPtr parseConstantExpr();
  ExprPtr parseStringLiteralExpr();
  ExprPtr parseParenExpr();

  DeclaratorPtr parseNonFunDeclarator();
  DeclaratorPtr parseDeclarator();
  CompStmtPtr parseCompoundStmt();
  ExternDeclPtr parseExternDecl();
};
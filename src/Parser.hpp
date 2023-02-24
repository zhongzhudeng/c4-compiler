#pragma once
#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "Sema.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "Token.hpp"
#include <functional>
#include <vector>

template <typename AST> class UnitTestParser;

class Parser {
public:
  Parser(std::vector<Token> token_stream, Sema &Action);
  ~Parser();
  void parse(std::function<void(DeclPtr, bool)> f);

private:
  std::vector<Token> token_stream;
  std::vector<Token>::iterator cursor;
  std::vector<Token>::const_iterator end;

  Sema &Action;

  friend UnitTestParser<Expr>;
  friend UnitTestParser<DeclaratorDecl>;
  friend UnitTestParser<Decl>;
  friend UnitTestParser<Stmt>;

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

  DeclSpecPtr parseDeclSpec();
  DeclaratorDeclPtr parseDecl();
  DeclaratorPtr parseDeclarator();
  DeclaratorPtr parseNonFuncDeclarator();

  StmtPtr parseStmt();

  ExprPtr parseUnaryExpr();
  ExprPtr parseBinaryExprRHS(int, ExprPtr);
  ExprPtr parsePostfixExprSuffix(ExprPtr expr);

  ExprPtr parseIdentifierExpr();
  ExprPtr parseConstantExpr();
  ExprPtr parseStringLiteralExpr();
  ExprPtr parseParenExpr();

  CompStmtPtr parseCompoundStmt();
  DeclPtr parseExternDecl();
};
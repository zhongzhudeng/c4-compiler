#pragma once
#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "Syntax.hpp"
#include "Token.hpp"
#include <utility>
#include <vector>

class Parser::impl {
public:
  impl(std::vector<Token> token_stream);
  ASTPtr parse();

private:
  std::vector<Token> token_stream;
  std::vector<Token>::iterator cursor;
  std::vector<Token>::const_iterator end;

  const Locatable &location(void);
  void bump(void);
  bool check(TokenKind tk);
  bool accept(TokenKind tk);
  void expect(TokenKind tk, const char *txt);
  TokenKind kind();
  TokenKind kind_first();
  bool is_paren_type_name();
  bool is_end();

  int precedence(TokenKind tk);

  ExprPtr parsePrimaryExpr();
  ExprPtr parseIdentifierExpr();
  ExprPtr parseConstantExpr();
  ExprPtr parseStringLiteralExpr();
  ExprPtr parseParenExpr();
  ExprPtr parsePostfixExpr();
  std::vector<ExprPtr> parserArgumentExpressionList();
  ExprPtr parseCastExpr();
  ExprPtr parseConditionalExpr(ExprPtr);
  ExprPtr parseBinaryExpr(int, ExprPtr);
  ExprPtr parseUnaryExpr();
  ExprPtr parseExpr();

  DeclarationPtr parseDeclaration();
  StructDeclarationPtr parseStructDeclaration();
  ParameterDeclarationPtr parseParameterDeclaration();

  DeclarationSpecifierPtr parseDeclarationSpecifier();
  TypeSpecifierPtr parseTypeSpecifier();
  StructSpecifierPtr parseStructSpecifier();
  SpecifierQualifierPtr parseSpecifierQualifier();
  StructDeclaratorPtr parseStructDeclarator();

  DeclaratorPtr parseDeclarator(DeclKind dKind);
  DeclaratorPtr parseNonFunDeclarator();
  
  StmtPtr parseStmt();
  std::vector<StmtPtr> parseBlockItemList();
};
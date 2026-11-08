#pragma once

#include "AST.hpp"
#include "Support.hpp"
#include "Token.hpp"

#include <cstddef>
#include <vector>

/// Recursive-descent parser for the restricted C4 grammar.  It performs
/// syntax analysis only and fills the ID-based AST; all names and types are
/// resolved later by Sema.
class Parser {
public:
  Parser(AST &ast, std::vector<Token> tokens, Diagnostics &diagnostics);

  void parseTranslationUnit();

private:
  AST &ast_;
  std::vector<Token> tokens_;
  size_t pos_ = 0;
  Diagnostics &diagnostics_;

  const Token &current() const;
  const Token &peekToken(size_t offset) const;
  TokenKind peekKind(size_t offset) const;
  SourceLoc loc() const;
  bool atEnd() const;
  bool check(TokenKind kind) const;
  bool accept(TokenKind kind);
  void expect(TokenKind kind, const char *what);
  void advance();
  [[noreturn]] void error(SourceLoc location, const std::string &message) const;

  bool isTypeSpecifierStart() const;
  bool isDeclaratorStart() const;
  bool isTypeNameAhead() const;
  bool isParameterListAhead() const;

  TypeSpecId parseTypeSpec();
  DeclaratorId parseDeclarator(bool allowName, bool allowEmpty);
  std::vector<DeclId> parseParameterList(SourceLoc lparen, bool &explicitVoid);
  DeclId parseParameterDeclaration();
  DeclId parseFieldDeclaration();

  DeclId parseExternalDeclaration();
  DeclId parseLocalDeclaration();

  StmtId parseStatement();
  StmtId parseCompoundStatement();

  ExprId parseExpression();
  ExprId parseConditional();
  ExprId parseBinary(int minPrecedence);
  ExprId parseUnary();
  ExprId parsePostfix();
  ExprId parsePrimary();

  bool isAssignmentLHS(ExprId id) const;
  static int binaryPrecedence(TokenKind kind, BinaryExpr::Op &op);
};

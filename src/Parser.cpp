#include "Parser.hpp"

#include <cassert>
#include <utility>

Parser::Parser(AST &ast, std::vector<Token> tokens, Diagnostics &diagnostics)
    : ast_(ast), tokens_(std::move(tokens)), diagnostics_(diagnostics) {
  assert(!tokens_.empty() && "lexer must provide an EOF token");
}

const Token &Parser::current() const {
  assert(pos_ < tokens_.size());
  return tokens_[pos_];
}

const Token &Parser::peekToken(size_t offset) const {
  size_t index = pos_ + offset;
  if (index >= tokens_.size())
    return tokens_.back();
  return tokens_[index];
}

TokenKind Parser::peekKind(size_t offset) const {
  return canonicalTokenKind(peekToken(offset).Kind);
}

SourceLoc Parser::loc() const { return current().Loc; }
bool Parser::atEnd() const { return current().Kind == TK_EOI; }

bool Parser::check(TokenKind kind) const {
  return canonicalTokenKind(current().Kind) == canonicalTokenKind(kind);
}

bool Parser::accept(TokenKind kind) {
  if (!check(kind))
    return false;
  advance();
  return true;
}

void Parser::advance() {
  if (!atEnd())
    ++pos_;
}

void Parser::expect(TokenKind kind, const char *what) {
  if (check(kind)) {
    advance();
    return;
  }
  if (atEnd())
    error(loc(), std::string("expected ") + what + " before end of input");
  std::string got = current().Text ? *current().Text : "";
  error(loc(), std::string("expected ") + what + " but got '" + got + "'");
}

void Parser::error(SourceLoc location, const std::string &message) const {
  diagnostics_.error(location, message);
}

bool Parser::isTypeSpecifierStart() const {
  return check(TK_VOID) || check(TK_CHAR) || check(TK_INT) ||
         check(TK_STRUCT);
}

bool Parser::isDeclaratorStart() const {
  return check(TK_STAR) || check(TK_LPAREN) || check(TK_IDENTIFIER);
}

bool Parser::isTypeNameAhead() const {
  // current is '('
  TokenKind next = peekKind(1);
  return next == TK_VOID || next == TK_CHAR || next == TK_INT ||
         next == TK_STRUCT;
}

bool Parser::isParameterListAhead() const {
  // current is '('; a parameter list follows if it is empty or begins with a
  // type specifier.  Otherwise this is a parenthesized declarator.
  TokenKind next = peekKind(1);
  return next == TK_RPAREN || next == TK_VOID || next == TK_CHAR ||
         next == TK_INT || next == TK_STRUCT;
}

TypeSpecId Parser::parseTypeSpec() {
  SourceLoc start = loc();
  if (accept(TK_VOID)) {
    TypeSpec spec;
    spec.kind = TypeSpec::Kind::Void;
    spec.loc = start;
    return ast_.addTypeSpec(std::move(spec));
  }
  if (accept(TK_CHAR)) {
    TypeSpec spec;
    spec.kind = TypeSpec::Kind::Char;
    spec.loc = start;
    return ast_.addTypeSpec(std::move(spec));
  }
  if (accept(TK_INT)) {
    TypeSpec spec;
    spec.kind = TypeSpec::Kind::Int;
    spec.loc = start;
    return ast_.addTypeSpec(std::move(spec));
  }
  if (accept(TK_STRUCT)) {
    TypeSpec spec;
    spec.kind = TypeSpec::Kind::Struct;
    spec.loc = start;
    if (check(TK_IDENTIFIER)) {
      spec.tag = current().Text;
      advance();
    }
    if (accept(TK_LBRACE)) {
      spec.hasBraces = true;
      while (!check(TK_RBRACE)) {
        if (atEnd())
          error(loc(), "expected '}' in struct declaration");
        spec.fields.push_back(parseFieldDeclaration());
      }
      expect(TK_RBRACE, "'}'");
    } else if (!spec.tag) {
      error(loc(), "expected struct tag or declaration");
    }
    return ast_.addTypeSpec(std::move(spec));
  }
  error(start, "expected type specifier");
}

DeclaratorId Parser::parseDeclarator(bool allowName, bool allowEmpty) {
  std::vector<SourceLoc> pointers;
  while (check(TK_STAR)) {
    pointers.push_back(loc());
    advance();
  }

  DeclaratorId direct;
  bool haveDirect = false;

  if (allowName && check(TK_IDENTIFIER)) {
    SourceLoc nameLoc = loc();
    Symbol name = current().Text;
    advance();
    direct = ast_.addDeclarator(nameLoc, NameDeclarator{name});
    haveDirect = true;
  } else if (check(TK_LPAREN)) {
    if (isParameterListAhead()) {
      SourceLoc lparen = loc();
      bool explicitVoid = false;
      std::vector<DeclId> params = parseParameterList(lparen, explicitVoid);
      direct = ast_.addDeclarator(
          lparen, FunctionDeclarator{DeclaratorId{}, std::move(params),
                                     explicitVoid});
      haveDirect = true;
    } else {
      advance(); // '('
      direct = parseDeclarator(allowName, true);
      expect(TK_RPAREN, "')'");
      haveDirect = true;
    }
  } else if (!allowEmpty) {
    error(loc(), "expected declarator");
  }

  while (check(TK_LPAREN)) {
    SourceLoc lparen = loc();
    bool explicitVoid = false;
    std::vector<DeclId> params = parseParameterList(lparen, explicitVoid);
    direct = ast_.addDeclarator(
        lparen, FunctionDeclarator{direct, std::move(params), explicitVoid});
    haveDirect = true;
  }

  for (auto it = pointers.rbegin(); it != pointers.rend(); ++it)
    direct = ast_.addDeclarator(*it, PointerDeclarator{direct});

  if (!haveDirect && pointers.empty())
    return DeclaratorId{};
  return direct;
}

std::vector<DeclId> Parser::parseParameterList(SourceLoc lparen,
                                               bool &explicitVoid) {
  (void)lparen;
  expect(TK_LPAREN, "'('");
  std::vector<DeclId> params;
  if (accept(TK_RPAREN))
    return params;
  if (check(TK_VOID) && peekKind(1) == TK_RPAREN) {
    advance(); // void
    advance(); // )
    explicitVoid = true;
    return params;
  }
  do {
    params.push_back(parseParameterDeclaration());
  } while (accept(TK_COMMA));
  expect(TK_RPAREN, "')' after parameter list");
  return params;
}

DeclId Parser::parseParameterDeclaration() {
  SourceLoc start = loc();
  TypeSpecId spec = parseTypeSpec();
  DeclaratorId declarator = parseDeclarator(true, true);
  return ast_.addDecl(start, ParamDecl{spec, declarator});
}

DeclId Parser::parseFieldDeclaration() {
  SourceLoc start = loc();
  TypeSpecId spec = parseTypeSpec();
  if (!isDeclaratorStart())
    error(loc(), "expected struct member declarator");
  DeclaratorId declarator = parseDeclarator(true, false);
  if (!declaratorHasName(ast_, declarator))
    error(start, "expected struct member name");
  expect(TK_SEMICOLON, "';' after struct member");
  return ast_.addDecl(start, FieldDecl{spec, declarator});
}

DeclId Parser::parseExternalDeclaration() {
  SourceLoc start = loc();
  TypeSpecId spec = parseTypeSpec();

  if (!isDeclaratorStart()) {
    const TypeSpec &ts = ast_.typeSpec(spec);
    if (ts.kind == TypeSpec::Kind::Struct) {
      expect(TK_SEMICOLON, "';' after struct declaration");
      return ast_.addDecl(start, TagDecl{spec});
    }
    error(loc(), "expected declarator");
  }

  DeclaratorId declarator = parseDeclarator(true, false);
  if (!declaratorHasName(ast_, declarator))
    error(start, "expected declarator name");

  if (check(TK_LBRACE)) {
    if (!declaratorProducesFunction(ast_, declarator))
      error(loc(), "function definition requires a function declarator");
    StmtId body = parseCompoundStatement();
    return ast_.addDecl(start, FunctionDecl{spec, declarator, body});
  }

  expect(TK_SEMICOLON, "';' after declaration");
  if (declaratorProducesFunction(ast_, declarator))
    return ast_.addDecl(start, FunctionDecl{spec, declarator, StmtId{}});
  return ast_.addDecl(start, VarDecl{spec, declarator});
}

DeclId Parser::parseLocalDeclaration() {
  SourceLoc start = loc();
  TypeSpecId spec = parseTypeSpec();

  if (!isDeclaratorStart()) {
    const TypeSpec &ts = ast_.typeSpec(spec);
    if (ts.kind == TypeSpec::Kind::Struct) {
      expect(TK_SEMICOLON, "';' after struct declaration");
      return ast_.addDecl(start, TagDecl{spec});
    }
    error(loc(), "expected declarator");
  }

  DeclaratorId declarator = parseDeclarator(true, false);
  if (!declaratorHasName(ast_, declarator))
    error(start, "expected declarator name");
  if (check(TK_LBRACE))
    error(loc(), "function definitions are only allowed at file scope");
  expect(TK_SEMICOLON, "';' after declaration");

  if (declaratorProducesFunction(ast_, declarator))
    return ast_.addDecl(start, FunctionDecl{spec, declarator, StmtId{}});
  return ast_.addDecl(start, VarDecl{spec, declarator});
}

StmtId Parser::parseStatement() {
  SourceLoc start = loc();

  if (check(TK_LBRACE))
    return parseCompoundStatement();

  if (accept(TK_IF)) {
    expect(TK_LPAREN, "'(' after if");
    ExprId cond = parseExpression();
    expect(TK_RPAREN, "')' after if condition");
    StmtId thenStmt = parseStatement();
    StmtId elseStmt;
    if (accept(TK_ELSE))
      elseStmt = parseStatement();
    return ast_.addStmt(start, IfStmt{cond, thenStmt, elseStmt});
  }

  if (accept(TK_WHILE)) {
    expect(TK_LPAREN, "'(' after while");
    ExprId cond = parseExpression();
    expect(TK_RPAREN, "')' after while condition");
    StmtId body = parseStatement();
    return ast_.addStmt(start, WhileStmt{cond, body});
  }

  if (accept(TK_GOTO)) {
    if (!check(TK_IDENTIFIER))
      error(loc(), "expected label name after goto");
    Symbol name = current().Text;
    advance();
    expect(TK_SEMICOLON, "';' after goto");
    return ast_.addStmt(start, GotoStmt{name});
  }

  if (accept(TK_CONTINUE)) {
    expect(TK_SEMICOLON, "';' after continue");
    return ast_.addStmt(start, ContinueStmt{});
  }

  if (accept(TK_BREAK)) {
    expect(TK_SEMICOLON, "';' after break");
    return ast_.addStmt(start, BreakStmt{});
  }

  if (accept(TK_RETURN)) {
    ExprId expr;
    if (!check(TK_SEMICOLON))
      expr = parseExpression();
    expect(TK_SEMICOLON, "';' after return");
    return ast_.addStmt(start, ReturnStmt{expr});
  }

  if (accept(TK_SEMICOLON))
    return ast_.addStmt(start, ExprStmt{ExprId{}});

  if (check(TK_IDENTIFIER) && peekKind(1) == TK_COLON) {
    Symbol name = current().Text;
    advance();
    advance(); // ':'
    LabelId label = ast_.addLabel(start, name);
    StmtId body = parseStatement();
    return ast_.addStmt(start, LabelStmt{label, body});
  }

  ExprId expr = parseExpression();
  expect(TK_SEMICOLON, "';' after expression statement");
  return ast_.addStmt(start, ExprStmt{expr});
}

StmtId Parser::parseCompoundStatement() {
  SourceLoc start = loc();
  expect(TK_LBRACE, "'{'");
  std::vector<StmtId> items;
  while (!check(TK_RBRACE)) {
    if (atEnd())
      error(loc(), "expected '}' before end of input");
    if (isTypeSpecifierStart())
      items.push_back(ast_.addStmt(loc(), DeclStmt{parseLocalDeclaration()}));
    else
      items.push_back(parseStatement());
  }
  expect(TK_RBRACE, "'}'");
  return ast_.addStmt(start, CompoundStmt{std::move(items)});
}

ExprId Parser::parseExpression() {
  ExprId lhs = parseConditional();
  if (check(TK_EQUAL)) {
    SourceLoc opLoc = loc();
    if (!isAssignmentLHS(lhs))
      error(opLoc, "left operand of assignment is not a unary expression");
    advance();
    ExprId rhs = parseExpression();
    return ast_.addExpr(opLoc, AssignExpr{lhs, rhs});
  }
  return lhs;
}

ExprId Parser::parseConditional() {
  ExprId cond = parseBinary(1);
  if (check(TK_QUESTION)) {
    SourceLoc opLoc = loc();
    advance();
    ExprId thenExpr = parseExpression();
    expect(TK_COLON, "':' in conditional expression");
    ExprId elseExpr = parseConditional();
    return ast_.addExpr(opLoc,
                        ConditionalExpr{cond, thenExpr, elseExpr});
  }
  return cond;
}

int Parser::binaryPrecedence(TokenKind kind, BinaryExpr::Op &op) {
  switch (canonicalTokenKind(kind)) {
  case TK_PIPEPIPE:
    op = BinaryExpr::Op::LogicalOr;
    return 1;
  case TK_AMPAMP:
    op = BinaryExpr::Op::LogicalAnd;
    return 2;
  case TK_EQUALEQUAL:
    op = BinaryExpr::Op::Equal;
    return 3;
  case TK_EXCLAIMEQUAL:
    op = BinaryExpr::Op::NotEqual;
    return 3;
  case TK_LESS:
    op = BinaryExpr::Op::Less;
    return 4;
  case TK_PLUS:
    op = BinaryExpr::Op::Add;
    return 5;
  case TK_MINUS:
    op = BinaryExpr::Op::Sub;
    return 5;
  case TK_STAR:
    op = BinaryExpr::Op::Mul;
    return 6;
  default:
    return 0;
  }
}

ExprId Parser::parseBinary(int minPrecedence) {
  ExprId lhs = parseUnary();
  for (;;) {
    BinaryExpr::Op op;
    int precedence = binaryPrecedence(current().Kind, op);
    if (precedence == 0 || precedence < minPrecedence)
      break;
    SourceLoc opLoc = loc();
    advance();
    ExprId rhs = parseBinary(precedence + 1);
    lhs = ast_.addExpr(opLoc, BinaryExpr{op, lhs, rhs});
  }
  return lhs;
}

ExprId Parser::parseUnary() {
  if (check(TK_AMP) || check(TK_STAR) || check(TK_MINUS) ||
      check(TK_EXCLAIM)) {
    SourceLoc opLoc = loc();
    UnaryExpr::Op op = UnaryExpr::Op::Plus;
    switch (canonicalTokenKind(current().Kind)) {
    case TK_AMP:
      op = UnaryExpr::Op::Addr;
      break;
    case TK_STAR:
      op = UnaryExpr::Op::Deref;
      break;
    case TK_MINUS:
      op = UnaryExpr::Op::Minus;
      break;
    case TK_EXCLAIM:
      op = UnaryExpr::Op::Not;
      break;
    default:
      break;
    }
    advance();
    ExprId operand = parseUnary();
    return ast_.addExpr(opLoc, UnaryExpr{op, operand});
  }

  if (check(TK_SIZEOF)) {
    SourceLoc sizeofLoc = loc();
    advance();
    if (check(TK_LPAREN) && isTypeNameAhead()) {
      advance(); // '('
      TypeSpecId spec = parseTypeSpec();
      DeclaratorId declarator = parseDeclarator(false, true);
      expect(TK_RPAREN, "')' after type name in sizeof");
      return ast_.addExpr(
          sizeofLoc, SizeofExpr{ExprId{}, spec, declarator, true});
    }
    ExprId operand = parseUnary();
    return ast_.addExpr(sizeofLoc,
                        SizeofExpr{operand, TypeSpecId{}, DeclaratorId{}, false});
  }

  return parsePostfix();
}

ExprId Parser::parsePostfix() {
  ExprId expr = parsePrimary();
  for (;;) {
    if (check(TK_LSQUARE)) {
      SourceLoc opLoc = loc();
      advance();
      ExprId index = parseExpression();
      expect(TK_RSQUARE, "']' after subscript");
      expr = ast_.addExpr(opLoc, SubscriptExpr{expr, index});
      continue;
    }
    if (check(TK_LPAREN)) {
      SourceLoc opLoc = loc();
      advance();
      std::vector<ExprId> args;
      if (!check(TK_RPAREN)) {
        do {
          args.push_back(parseExpression());
        } while (accept(TK_COMMA));
      }
      expect(TK_RPAREN, "')' after argument list");
      expr = ast_.addExpr(opLoc, CallExpr{expr, std::move(args)});
      continue;
    }
    if (check(TK_PERIOD) || check(TK_ARROW)) {
      bool arrow = check(TK_ARROW);
      SourceLoc opLoc = loc();
      advance();
      if (!check(TK_IDENTIFIER))
        error(loc(), "expected member name");
      Symbol member = current().Text;
      advance();
      expr = ast_.addExpr(opLoc, MemberExpr{expr, member, arrow});
      continue;
    }
    break;
  }
  return expr;
}

ExprId Parser::parsePrimary() {
  SourceLoc start = loc();
  if (check(TK_IDENTIFIER)) {
    Symbol name = current().Text;
    advance();
    return ast_.addExpr(start, IdentifierExpr{name});
  }
  if (check(TK_CONSTANT)) {
    Symbol spelling = current().Text;
    advance();
    return ast_.addExpr(start, ConstantExpr{spelling});
  }
  if (check(TK_STRING_LITERAL)) {
    Symbol spelling = current().Text;
    advance();
    return ast_.addExpr(start, StringLiteralExpr{spelling});
  }
  if (check(TK_LPAREN)) {
    advance();
    ExprId expr = parseExpression();
    expect(TK_RPAREN, "')'");
    return expr; // parentheses are not represented in the AST
  }
  error(start, "expected expression");
}

bool Parser::isAssignmentLHS(ExprId id) const {
  const Expr &expr = ast_.expr(id);
  return !std::holds_alternative<BinaryExpr>(expr.node) &&
         !std::holds_alternative<ConditionalExpr>(expr.node) &&
         !std::holds_alternative<AssignExpr>(expr.node);
}

void Parser::parseTranslationUnit() {
  if (atEnd())
    error(loc(), "expected external declaration");
  while (!atEnd())
    ast_.translationUnit().decls.push_back(parseExternalDeclaration());
}

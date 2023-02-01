#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxExpr.hpp"
#include "Token.hpp"
#include <memory>
#include <vector>

#include "FirstFollow.def"

enum Level {
  Unknown = 0,
  Assignment = 1,
  Conditional = 2,
  LogicalOr = 3,
  LogicalAnd = 4,
  Equality = 5,
  Relational = 6,
  Additive = 7,
  Multiplicative = 8,
};

Level precedence(TokenKind tk) {
  switch (tk) {
  default:
    return Unknown;
  case TK_EQUAL:
    return Assignment;
  case TK_QUESTION:
    return Conditional;
  case TK_PIPEPIPE:
    return LogicalOr;
  case TK_AMPAMP:
    return LogicalAnd;
  case TK_EQUALEQUAL:
  case TK_EXCLAIMEQUAL:
    return Equality;
  case TK_LESS:
    return Relational;
  case TK_MINUS:
  case TK_PLUS:
    return Additive;
  case TK_STAR:
    return Multiplicative;
  }
}

ExprPtr Parser::parsePostfixExprSuffix(ExprPtr expr) {
  switch (kind()) {
  default:
    if (is_end())
      return expr;
    errorloc(location(), "Expect '[', '(', '.', '->' in postfix expression");
  case TK_LSQUARE: {
    Locatable loc(location());
    bump();
    auto expr_in_square = parseExpr();
    expect(TK_RSQUARE, "']' in array subscript");
    return std::make_unique<ArraySubscriptExpr>(loc, std::move(expr),
                                                std::move(expr_in_square));
  }
  case TK_LPAREN: {
    Locatable loc(location());
    bump();
    std::vector<ExprPtr> args;
    while (!accept(TK_RPAREN)) {
      /// TODO: check token kinds?
      auto arg = parseExpr();
      args.push_back(std::move(arg));
      if (accept(TK_RPAREN))
        break;
      expect(TK_COMMA, "')' or ',' in argument list");
    }
    return std::make_unique<FunctionCallExpr>(loc, std::move(expr),
                                              std::move(args));
  }
  case TK_PERIOD:
  case TK_ARROW: {
    Locatable loc(location());
    Symbol acccesOp = cursor->Text;
    bump();
    auto iden = parseIdentifierExpr();
    return std::make_unique<MemberExpr>(loc, std::move(expr), std::move(iden),
                                        acccesOp);
  }
    POST_FOLLOW
    return expr;
  }
}

ExprPtr Parser::parseUnaryExpr() {
  ExprPtr expr = nullptr;

  switch (kind()) {
  default:
    errorloc(location(), "Unknown token ", *cursor->Text,
             " when expecting unary expression");
  case TK_LPAREN:
    expr = parseParenExpr();
    break;
  case TK_IDENTIFIER:
    expr = parseIdentifierExpr();
    break;
  case TK_CONSTANT:
    expr = parseConstantExpr();
    break;
  case TK_STRING_LITERAL:
    expr = parseStringLiteralExpr();
    break;
  case TK_AMP:
  case TK_STAR:
  case TK_MINUS:
  case TK_EXCLAIM: {
    Locatable loc(location());
    Symbol Op = cursor->Text;
    bump();
    expr = parseUnaryExpr();
    expr = std::make_unique<UnaryExpr>(loc, Op, std::move(expr));
    break;
  }
  case TK_SIZEOF: {
    // TODO: parse type-name
    Locatable loc(location());
    bump();
    if (is_paren_type_name()) {
      Symbol type_name = (cursor + 1)->Text;
      bump(), bump();
      expect(TK_RPAREN, "')' after type-name in sizeof (type-name)");
      return std::make_unique<SizeofTypeExpr>(loc, type_name);
    }
    return std::make_unique<SizeofExpr>(loc, parseUnaryExpr());
    break;
  }
  }
  expr = parsePostfixExprSuffix(std::move(expr));
  return expr;
}

ExprPtr Parser::parseExpr() {
  ExprPtr LHS = parseUnaryExpr();
  return parseBinaryExprRHS(Assignment, std::move(LHS));
}

ExprPtr Parser::parseIdentifierExpr() {
  Locatable loc(location());
  Symbol identifier = cursor->Text;
  bump();
  return std::make_unique<IdentifierExpr>(loc, identifier);
}

ExprPtr Parser::parseConstantExpr() {
  Locatable loc(location());
  Symbol val = cursor->Text;
  bump();
  return std::make_unique<ConstantExpr>(loc, val);
}
ExprPtr Parser::parseStringLiteralExpr() {
  Locatable loc(location());
  Symbol val = cursor->Text;
  bump();
  return std::make_unique<StringLiteralExpr>(loc, val);
}
ExprPtr Parser::parseParenExpr() {
  Locatable loc(location());
  expect(TK_LPAREN, "'('");
  auto expr = parseExpr();
  expect(TK_RPAREN, "')'");
  if (expr == nullptr)
    return expr;
  return std::make_unique<ParenExpr>(loc, std::move(expr));
}

ExprPtr Parser::parseBinaryExprRHS(int exprPrec, ExprPtr LHS) {
  for (;;) {
    ExprPtr middle = nullptr;
    int TokPrec = precedence(cursor->Kind);
    if (TokPrec < exprPrec)
      return LHS;
    // Special case for conditional expression
    Symbol BinOp = cursor->Text;
    bump();
    if (TokPrec == Conditional) {
      middle = parseExpr();
      expect(TK_COLON, "':' in conditional expression");
    }
    ExprPtr RHS = parseUnaryExpr();
    if (!RHS)
      return 0;
    int NextPrec = precedence(cursor->Kind);
    if (TokPrec < NextPrec) {
      RHS = parseBinaryExprRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }
    if (middle != nullptr) {
      LHS = std::make_unique<ConditionalExpr>(
          LHS->getLoc(), std::move(LHS), std::move(middle), std::move(RHS));
    } else {
      LHS = std::make_unique<BinaryExpr>(LHS->getLoc(), BinOp, std::move(LHS),
                                         std::move(RHS));
    }
  }
}
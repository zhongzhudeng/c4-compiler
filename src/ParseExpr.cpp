#include "Diagnostic.hpp"
#include "ParserImpl.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxExpr.hpp"
#include "Token.hpp"
#include <utility>
#include <vector>

#include "FirstFollow.def"

int Parser::impl::precedence(TokenKind tk) {
  switch (tk) {
  case TK_STAR:
    return 5;
  case TK_PLUS:
  case TK_MINUS:
    return 4;
  case TK_LESS:
    return 3;
  case TK_EQUALEQUAL:
  case TK_EXCLAIMEQUAL:
    return 2;
  case TK_AMPAMP:
    return 1;
  case TK_PIPEPIPE:
    return 0;
  default:
    return -1;
  }
}

ExprPtr Parser::impl::parsePrimaryExpr() {
  switch (kind()) {
  default:
    errorloc(location(), "Unknown token ", cursor->Text,
             " when expecting primary expression\n");
  case TK_IDENTIFIER:
    return parseIdentifierExpr();
  case TK_CONSTANT:
    return parseConstantExpr();
  case TK_STRING_LITERAL:
    return parseStringLiteralExpr();
  case TK_LPAREN:
    return parseParenExpr();
  }
}

ExprPtr Parser::impl::parsePostfixExpr() {
  ExprPtr expr;
  switch (kind()) {
  default:
    errorloc(location(), "Unknown token ", cursor->Text,
             " when expecting postfix expression");
    PRIMARY
    expr = parsePrimaryExpr();
    break;
  }
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
    auto args = parserArgumentExpressionList();
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

std::vector<ExprPtr> Parser::impl::parserArgumentExpressionList() {
  std::vector<ExprPtr> args;
  while (!accept(TK_RPAREN)) {
    /// TODO: check token kinds?
    auto arg = parseExpr();
    args.push_back(std::move(arg));
    if (check(TK_RPAREN))
      return args;
    expect(TK_COMMA, "')' or ',' in argument list");
  }
  return args;
}

ExprPtr Parser::impl::parseUnaryExpr() {
  switch (kind()) {
  default:
    errorloc(location(), "Unknown token ", cursor->Text,
             " when expecting unary expression");
    PRIMARY
    return parsePostfixExpr();
    UNARY_OP_NO_SIZEOF {
      Locatable loc(location());
      Symbol UnaryOp = cursor->Text;
      bump();
      ExprPtr expr = parseUnaryExpr();
      return std::make_unique<UnaryExpr>(loc, UnaryOp, std::move(expr));
    }
  case TK_SIZEOF: {
    Locatable loc(location());
    bump();
    if (is_paren_type_name()) {
      Symbol type_name = (cursor + 1)->Text;
      bump(), bump();
      return std::make_unique<SizeofTypeExpr>(loc, type_name);
    }
    return std::make_unique<SizeofExpr>(loc, parseUnaryExpr());
  }
  }
}

ExprPtr Parser::impl::parseCastExpr() {
  switch (kind()) {
  default:
    errorloc(location(), "Unknown token ", cursor->Text,
             " when expecting cast expression");
    PRIMARY_NO_LPAREN
    UNARY_OP
    return parseUnaryExpr();
  case TK_LPAREN: {
    switch (kind_first()) {
    default:
      return parseUnaryExpr();
      TYPENAME {
        Locatable loc(location());
        bump();
        Symbol type_name = cursor->Text;
        expect(TK_RPAREN, "')'");
        return std::make_unique<CastExpr>(loc, type_name, parseCastExpr());
      }
    }
  }
  }
}

ExprPtr Parser::impl::parseExpr() {
  ExprPtr LHS;
  if (is_paren_type_name()) {
    LHS = parseCastExpr();
    if (is_end())
      return LHS;
    else
      return parseConditionalExpr(std::move(LHS));
  } else {
    switch (kind()) {
    default:
      errorloc(location(), "Unknown token ", cursor->Text,
               " when expecting an expression");
      PRIMARY
      UNARY_OP
      LHS = parseUnaryExpr();
      if (is_end())
        return LHS;
      else {
        if (kind() == TK_EQUAL) {
          bump();
          auto RHS = parseExpr();
          return std::make_unique<AssignmentExpr>(LHS->getLoc(), std::move(LHS),
                                                  std::move(RHS));
        } else {
          return parseConditionalExpr(std::move(LHS));
        }
      }
    }
  }
}

ExprPtr Parser::impl::parseConditionalExpr(ExprPtr expr) {
  switch (kind()) {
  default:
    if (is_end())
      return expr;
    errorloc(location(),
             "Unknown token when expecting a conditional expression");
  case TK_QUESTION: {
    bump();
    auto second = parseExpr();
    expect(TK_COLON, "':' in conditional expression");
    auto first_in_third = parseCastExpr();
    auto third = parseConditionalExpr(std::move(first_in_third));
    return std::make_unique<ConditionalExpr>(
        expr->getLoc(), std::move(expr), std::move(second), std::move(third));
  }
    BINARY_OP {
      auto binaryExpr = parseBinaryExpr(0, std::move(expr));
      switch (kind()) {
      default:
        if (is_end())
          return binaryExpr;
        errorloc(location(),
                 "Unknown token when expecting a conditional expression");
      case TK_RPAREN:
      case TK_RSQUARE:
      case TK_COLON:
      case TK_COMMA:
        return binaryExpr;
      case TK_QUESTION: {
        bump();
        auto second = parseExpr();
        expect(TK_COLON, "':' in conditional expression");
        auto first_in_third = parseCastExpr();
        auto third = parseConditionalExpr(std::move(first_in_third));
        return std::make_unique<ConditionalExpr>(
            binaryExpr->getLoc(), std::move(binaryExpr), std::move(second),
            std::move(third));
      }
      }
    }

  case TK_RPAREN:
  case TK_RSQUARE:
  case TK_COLON:
  case TK_COMMA:
    return expr;
  }
}

ExprPtr Parser::impl::parseIdentifierExpr() {
  Locatable loc(location());
  Symbol identifier = cursor->Text;
  bump();
  return std::make_unique<IdentifierExpr>(loc, identifier);
}

ExprPtr Parser::impl::parseConstantExpr() {
  Locatable loc(location());
  Symbol val = cursor->Text;
  bump();
  return std::make_unique<ConstantExpr>(loc, val);
}
ExprPtr Parser::impl::parseStringLiteralExpr() {
  Locatable loc(location());
  Symbol val = cursor->Text;
  bump();
  return std::make_unique<StringLiteralExpr>(loc, val);
}
ExprPtr Parser::impl::parseParenExpr() {
  Locatable loc(location());
  bump();
  auto expr = parseExpr();
  if (!expr)
    return nullptr;
  expect(TK_RPAREN, "')'");
  return std::make_unique<ParenExpr>(loc, std::move(expr));
}

ExprPtr Parser::impl::parseBinaryExpr(int exprPrec, ExprPtr LHS) {
  for (;;) {
    int TokPrec = precedence(cursor->Kind);
    if (TokPrec < exprPrec)
      return LHS;
    Symbol BinOp = cursor->Text;
    bump();
    ExprPtr RHS = parseCastExpr();
    if (!RHS)
      return 0;
    int NextPrec = precedence(cursor->Kind);
    if (TokPrec < NextPrec) {
      RHS = parseBinaryExpr(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }
    LHS = std::make_unique<BinaryExpr>(LHS->getLoc(), BinOp, std::move(LHS),
                                       std::move(RHS));
  }
}
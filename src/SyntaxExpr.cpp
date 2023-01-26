#include "SyntaxExpr.hpp"
#include "Diagnostic.hpp"
#include "Syntax.hpp"

Expr::Expr(Locatable loc, Kind kind) : loc(loc), kind(kind) {}
const Locatable &Expr::getLoc() { return loc; }
Expr::Kind Expr::getKind() const { return kind; }

IdentifierExpr::IdentifierExpr(Locatable loc, Symbol identifier)
    : Expr(loc, Expr::Identifier), identifier(identifier) {}
ConstantExpr::ConstantExpr(Locatable loc, Symbol val)
    : Expr(loc, Expr::Constant), val(val) {}
StringLiteralExpr::StringLiteralExpr(Locatable loc, Symbol val)
    : Expr(loc, Expr::StringLiteral), val(val) {}
ParenExpr::ParenExpr(Locatable loc, ExprPtr expr)
    : Expr(loc, Expr::Paren), expr(std::move(expr)) {}
ConditionalExpr::ConditionalExpr(Locatable loc, ExprPtr first, ExprPtr second,
                                 ExprPtr third)
    : Expr(loc, Expr::Conditional), first(std::move(first)),
      second(std::move(second)), third(std::move(third)) {}
BinaryExpr::BinaryExpr(Locatable loc, Symbol Op, ExprPtr LHS, ExprPtr RHS)
    : Expr(loc, Expr::Binary), BinaryOp(Op), LHS(std::move(LHS)),
      RHS(std::move(RHS)) {}
UnaryExpr::UnaryExpr(Locatable loc, Symbol UnaryOp, ExprPtr expr)
    : Expr(loc, Expr::Unary), UnaryOp(UnaryOp), expr(std::move(expr)) {}
SizeofExpr::SizeofExpr(Locatable loc, ExprPtr expr)
    : Expr(loc, Expr::Sizeof), expr(std::move(expr)) {}
SizeofTypeExpr::SizeofTypeExpr(Locatable loc, Symbol type_name)
    : Expr(loc, Expr::SizeofType), type_name(type_name) {}
ArraySubscriptExpr::ArraySubscriptExpr(Locatable loc, ExprPtr expr,
                                       ExprPtr squaredExpr)
    : Expr(loc, Expr::ArraySubscript), expr(std::move(expr)),
      squaredExpr(std::move(squaredExpr)) {}
FunctionCallExpr::FunctionCallExpr(Locatable loc, ExprPtr callee,
                                   std::vector<ExprPtr> args)
    : Expr(loc, Expr::FunctionCall), callee(std::move(callee)),
      args(std::move(args)) {}
MemberExpr::MemberExpr(Locatable loc, ExprPtr expr, ExprPtr member,
                       Symbol acccesOp)
    : Expr(loc, Expr::Member), expr(std::move(expr)), member(std::move(member)),
      acccesOp(acccesOp) {}

Symbol IdentifierExpr::getIdentifier() const { return identifier; }
Symbol ConstantExpr::getConstant() const { return val; };
Symbol StringLiteralExpr::getStringLiteral() const { return val; };
const Expr &ParenExpr::getExpr() const { return const_cast<Expr &>(*expr); }

const Expr &ConditionalExpr::getFirst() const {
  return const_cast<Expr &>(*first);
}
const Expr &ConditionalExpr::getSecond() const {
  return const_cast<Expr &>(*second);
}
const Expr &ConditionalExpr::getThird() const {
  return const_cast<Expr &>(*third);
}
const Expr &BinaryExpr::getLHS() const { return const_cast<Expr &>(*LHS); }
Symbol BinaryExpr::getOperator() const { return BinaryOp; }
const Expr &BinaryExpr::getRhS() const { return const_cast<Expr &>(*RHS); }
Symbol UnaryExpr::getOperator() const { return UnaryOp; }
const Expr &UnaryExpr::getExpr() const { return const_cast<Expr &>(*expr); }
const Expr &SizeofExpr::getExpr() const { return const_cast<Expr &>(*expr); }
Symbol SizeofTypeExpr::getTypeName() const { return type_name; }

const Expr &ArraySubscriptExpr::getExpr() const {
  return const_cast<Expr &>(*expr);
}
const Expr &ArraySubscriptExpr::getSquaredExpr() const {
  return const_cast<Expr &>(*squaredExpr);
}
const Expr &FunctionCallExpr::getCallee() const {
  return const_cast<Expr &>(*callee);
}
const std::vector<ExprPtr> &FunctionCallExpr::getArgs() const { return args; }
Symbol MemberExpr::getAccessOp() const { return acccesOp; }
const Expr &MemberExpr::getExpr() const { return const_cast<Expr &>(*expr); }
const Expr &MemberExpr::getMember() const {
  return const_cast<Expr &>(*member);
}
#include "SyntaxExpr.hpp"
#include "Printer.hpp"
Expr::Expr(Locatable loc) : AST(loc) {}
Expr::~Expr() {}

IdentifierExpr::IdentifierExpr(Locatable loc, Symbol identifier)
    : Expr(loc), identifier(identifier) {}
ConstantExpr::ConstantExpr(Locatable loc, Symbol val) : Expr(loc), val(val) {}
StringLiteralExpr::StringLiteralExpr(Locatable loc, Symbol val)
    : Expr(loc), val(val) {}
ParenExpr::ParenExpr(Locatable loc, ExprPtr expr)
    : Expr(loc), expr(std::move(expr)) {}
AssignmentExpr::AssignmentExpr(Locatable loc, ExprPtr LHS, ExprPtr RHS)
    : Expr(loc), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
ConditionalExpr::ConditionalExpr(Locatable loc, ExprPtr first, ExprPtr second,
                                 ExprPtr third)
    : Expr(loc), first(std::move(first)), second(std::move(second)),
      third(std::move(third)) {}
BinaryExpr::BinaryExpr(Locatable loc, Symbol Op, ExprPtr LHS, ExprPtr RHS)
    : Expr(loc), BinaryOp(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
UnaryExpr::UnaryExpr(Locatable loc, Symbol UnaryOp, ExprPtr expr)
    : Expr(loc), UnaryOp(UnaryOp), expr(std::move(expr)) {}
SizeofExpr::SizeofExpr(Locatable loc, ExprPtr expr)
    : Expr(loc), expr(std::move(expr)) {}
SizeofTypeExpr::SizeofTypeExpr(Locatable loc, Symbol type_name)
    : Expr(loc), type_name(type_name) {}
CastExpr::CastExpr(Locatable loc, Symbol type_name, ExprPtr expr)
    : Expr(loc), type_name(type_name), expr(std::move(expr)) {}
ArraySubscriptExpr::ArraySubscriptExpr(Locatable loc, ExprPtr expr,
                                       ExprPtr squaredExpr)
    : Expr(loc), expr(std::move(expr)), squaredExpr(std::move(squaredExpr)) {}
FunctionCallExpr::FunctionCallExpr(Locatable loc, ExprPtr callee,
                                   std::vector<ExprPtr> args)
    : Expr(loc), callee(std::move(callee)), args(std::move(args)) {}
MemberExpr::MemberExpr(Locatable loc, ExprPtr expr, ExprPtr member,
                       Symbol acccesOp)
    : Expr(loc), expr(std::move(expr)), member(std::move(member)),
      acccesOp(acccesOp) {}

void IdentifierExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void ConstantExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void StringLiteralExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void ParenExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void AssignmentExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void ConditionalExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void BinaryExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void UnaryExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void SizeofExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void SizeofTypeExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void CastExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void ArraySubscriptExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void FunctionCallExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void MemberExpr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }

Symbol IdentifierExpr::getIdentifier() const { return identifier; }
Symbol ConstantExpr::getConstant() const { return val; };
Symbol StringLiteralExpr::getStringLiteral() const { return val; };
const Expr &ParenExpr::getExpr() const { return const_cast<Expr &>(*expr); }
const Expr &AssignmentExpr::getLHS() const { return const_cast<Expr &>(*LHS); }
const Expr &AssignmentExpr::getRHS() const { return const_cast<Expr &>(*RHS); }
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
Symbol CastExpr::getTypeName() const { return type_name; }
const Expr &CastExpr::getExpr() const { return const_cast<Expr &>(*expr); }

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
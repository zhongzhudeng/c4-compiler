#include "Printer.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxDecl.hpp"
#include <sstream>
#include <vector>

std::ostream &operator<<(std::ostream &os, const AST &ast) {
  ASTPrinter printer(os);
  printer.Visit(ast);
  return os;
}

std::ostream &operator<<(std::ostream &os, Symbol symbol) {
  os << *symbol;
  return os;
}

ASTPrinter::ASTPrinter(std::ostream &os) : os(os) {}

void ASTPrinter::Visit(const AST &ast) { const_cast<AST &>(ast).accept(*this); }

void ASTPrinter::Visit(const IdentifierExpr &expr) {
  os << expr.getIdentifier();
}
void ASTPrinter::Visit(const ConstantExpr &expr) { os << expr.getConstant(); }
void ASTPrinter::Visit(const StringLiteralExpr &expr) {
  os << expr.getStringLiteral();
}
void ASTPrinter::Visit(const ParenExpr &expr) { Visit(expr.getExpr()); }
void ASTPrinter::Visit(const AssignmentExpr &expr) {
  os << "(";
  Visit(expr.getLHS());
  os << " = ";
  Visit(expr.getRHS());
  os << ")";
}
void ASTPrinter::Visit(const ConditionalExpr &expr) {
  os<<"(";
  Visit(expr.getFirst());
  os << " ? ";
  Visit(expr.getSecond());
  os << " : ";
  Visit(expr.getThird());
  os<<")";
}
void ASTPrinter::Visit(const BinaryExpr &expr) {
  os << "(";
  Visit(expr.getLHS());
  os << ' ' << expr.getOperator() << ' ';
  Visit(expr.getRhS());
  os << ")";
}
void ASTPrinter::Visit(const UnaryExpr &expr) {
  os << "(";
  os << expr.getOperator();
  Visit(expr.getExpr());
  os << ")";
}
void ASTPrinter::Visit(const SizeofExpr &expr) {
  os << "(sizeof ";
  Visit(expr.getExpr());
  os << ")";
}
void ASTPrinter::Visit(const SizeofTypeExpr &expr) {
  os << "(sizeof(" << expr.getTypeName() << "))";
}
void ASTPrinter::Visit(const ArraySubscriptExpr &expr) {
  os << "(";
  Visit(expr.getExpr());
  os << "[";
  Visit(expr.getSquaredExpr());
  os << "])";
}
void ASTPrinter::Visit(const FunctionCallExpr &expr) {
  os << "(";
  Visit(expr.getCallee());
  os << "(";
  const std::vector<ExprPtr> &args = expr.getArgs();
  for (auto it = args.begin(); it != args.end() - 1; it++) {
    const Expr &arg = const_cast<Expr &>(**it);
    Visit(arg);
    os << ", ";
  }
  const Expr &arg = const_cast<Expr &>(**(args.end() - 1));
  Visit(arg);
  os << "))";
}
void ASTPrinter::Visit(const MemberExpr &expr) {
  os << "(";
  Visit(expr.getExpr());
  os << expr.getAccessOp();
  Visit(expr.getMember());
  os << ")";
}
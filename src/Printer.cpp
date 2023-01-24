#include "Printer.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"
#include "SyntaxExternalDecl.hpp"
#include <cstddef>
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

void ASTPrinter::increaseIndent(){
  indent++;
}
void ASTPrinter::decreaseIndent(){
  indent--;
}
void ASTPrinter::nextLine(){
  os << "\n";
  for (int i = 0; i < indent; i++) {
    os << "\t";
  }
}

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
void ASTPrinter::Visit(const Stmt &stmt){
};
void ASTPrinter::Visit(const IdentifierStmt &stmt){
  os << stmt.getIdentifier();
  os << " : ";
  Visit(stmt.getStmt());
};
void ASTPrinter::Visit(const CompStmt &stmt){
  os << "{";
  increaseIndent();
  /*auto bil = stmt.getBlockItemList();
  for (size_t i = 0; i < bil.size(); i++) {
      nextLine();
      Visit(*(bil[i]));
  }*/
  decreaseIndent();
  nextLine();
  os << "}";
};
void ASTPrinter::Visit(const ExprStmt &stmt){
  if (stmt.getHasExpr()){
    Visit(stmt.getExpr());
  }
  os << ";";
};
void ASTPrinter::Visit(const IfStmt &stmt){
  os << "if (";
  Visit(stmt.getCond());
  os << ") ";
  increaseIndent();
  Visit(stmt.getIf());
  decreaseIndent();
  nextLine();
};
void ASTPrinter::Visit(const IfElseStmt &stmt){
  os << "if (";
  Visit(stmt.getCond());
  os << ") ";
  increaseIndent();
  Visit(stmt.getIf());
  decreaseIndent();
  nextLine();
  os << "else ";
  increaseIndent();
  Visit(stmt.getElse());
  decreaseIndent();
  nextLine();
};
void ASTPrinter::Visit(const WhileStmt &stmt){
  os << "while (";
  Visit(stmt.getCond());
  os << ") ";
  increaseIndent();
  Visit(stmt.getDo());
  decreaseIndent();
  nextLine();
};
void ASTPrinter::Visit(const GoToStmt &stmt){
  os << "goto ";
  os << stmt.getIdentifier();
};
void ASTPrinter::Visit(const ContinueStmt &stmt){
  os << "continue;";
};
void ASTPrinter::Visit(const BreakStmt &stmt){
  os << "break;";
};

void ASTPrinter::Visit(const ExternalDecl &externalDecl){
  //Visit(externalDecl.getDecl());
  if (externalDecl.hasComp()){
    os << " ";
    Visit(externalDecl.getStmt());
  }
}

void ASTPrinter::Visit(const TranslationUnit &tu){
  /*std::vector<ExternalDeclPtr> bil = tu.getExternalDecl();
  for (size_t i = 0; i < bil.size(); i++) {
      nextLine();
      Visit(*(bil[i]));
  }*/
}
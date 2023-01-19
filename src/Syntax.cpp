#include "Syntax.hpp"
#include "Printer.hpp"

AST::AST(Locatable loc) : loc(loc) {}
AST::~AST() {}
const Locatable &AST::getLoc() { return loc; }

Expr::Expr(Locatable loc) : AST(loc) {}
void Expr::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
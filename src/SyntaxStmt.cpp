#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include <ostream>
#include <sstream>
#include <utility>

Stmt::~Stmt() {}

IdentifierStmt::IdentifierStmt(Symbol identifier, StmtPtr stmt) 
    : identifier(identifier), stmt(std::move(stmt)) {}
CompStmt::CompStmt(std::vector<StmtPtr> blockItemList) 
    : blockItemList(std::move(blockItemList)) {};
ExprStmt::ExprStmt(ExprPtr expr) : expr(std::move(expr)) {};
IfStmt::IfStmt(ExprPtr cond, StmtPtr ifSt) 
    : cond(std::move(cond)), ifSt(std::move(ifSt)) {};
IfElseStmt::IfElseStmt(ExprPtr cond, StmtPtr ifSt, StmtPtr elseSt) 
    : cond(std::move(cond)), ifSt(std::move(ifSt)), elseSt(std::move(elseSt)) {};
WhileStmt::WhileStmt(ExprPtr cond, StmtPtr doSt) 
    : cond(std::move(cond)), doSt(std::move(doSt)) {};
GoToStmt::GoToStmt(Symbol identifier) 
    : identifier(identifier) {};
ContinueStmt::ContinueStmt(){};
BreakStmt::BreakStmt(){};

void IdentifierStmt::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void CompStmt::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void ExprStmt::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void IfStmt::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void IfElseStmt::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void WhileStmt::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void GoToStmt::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void ContinueStmt::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void BreakStmt::accept(ASTPrinter &visitor) { visitor.Visit(*this); }

Symbol IdentifierStmt::getIdentifier() const { return identifier; };
const Stmt &IdentifierStmt::getStmt() const { return const_cast<Stmt &>(*stmt); };
const std::vector<StmtPtr> &CompStmt::getBlockItemList() const { return const_cast<std::vector<StmtPtr> &>(blockItemList); };
const Expr &ExprStmt::getExpr() const { return const_cast<Expr &>(*expr); };
const Expr &IfStmt::getCond() const { return const_cast<Expr &>(*cond); };
const Stmt &IfStmt::getIf() const { return const_cast<Stmt &>(*ifSt); };
const Expr &IfElseStmt::getCond() const { return const_cast<Expr &>(*cond); };
const Stmt &IfElseStmt::getIf() const { return const_cast<Stmt &>(*ifSt); };
const Stmt &IfElseStmt::getElse() const { return const_cast<Stmt &>(*elseSt); };
const Expr &WhileStmt::getCond() const { return const_cast<Expr &>(*cond); };
const Stmt &WhileStmt::getDo() const { return const_cast<Stmt &>(*doSt); };
Symbol GoToStmt::getIdentifier() const { return identifier; };
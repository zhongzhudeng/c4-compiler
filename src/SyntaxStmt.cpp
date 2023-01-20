#include "SymbolInternalizer.hpp"
#include "SyntaxStmt.hpp"
#include <ostream>
#include <sstream>
#include <utility>

IdentifierStmt::IdentifierStmt(Locatable loc, Symbol identifier, StmtPtr stmt) 
    : Stmt(loc), identifier(identifier), stmt(std::move(stmt)) {}
CompStmt::CompStmt(Locatable loc, std::vector<ASTPtr> blockItemList) 
    : Stmt(loc), blockItemList(std::move(blockItemList)) {};
ExprStmt::ExprStmt(Locatable loc, ExprPtr expr) : Stmt(loc), expr(std::move(expr)) {};
IfStmt::IfStmt(Locatable loc, ExprPtr cond, StmtPtr ifSt) 
    : Stmt(loc), cond(std::move(cond)), ifSt(std::move(ifSt)) {};
IfElseStmt::IfElseStmt(Locatable loc, ExprPtr cond, StmtPtr ifSt, StmtPtr elseSt) 
    : Stmt(loc), cond(std::move(cond)), ifSt(std::move(ifSt)), elseSt(std::move(elseSt)) {};
WhileStmt::WhileStmt(Locatable loc, ExprPtr cond, StmtPtr doSt) 
    : Stmt(loc), cond(std::move(cond)), doSt(std::move(doSt)) {};
GoToStmt::GoToStmt(Locatable loc, Symbol identifier) 
    : Stmt(loc), identifier(identifier) {};
ContinueStmt::ContinueStmt(Locatable loc): Stmt(loc){};
BreakStmt::BreakStmt(Locatable loc): Stmt(loc){};

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
const std::vector<ASTPtr> &CompStmt::getBlockItemList() const { return const_cast<std::vector<ASTPtr> &>(blockItemList); };
const Expr &ExprStmt::getExpr() const { return const_cast<Expr &>(*expr); };
const Expr &IfStmt::getCond() const { return const_cast<Expr &>(*cond); };
const Stmt &IfStmt::getIf() const { return const_cast<Stmt &>(*ifSt); };
const Expr &IfElseStmt::getCond() const { return const_cast<Expr &>(*cond); };
const Stmt &IfElseStmt::getIf() const { return const_cast<Stmt &>(*ifSt); };
const Stmt &IfElseStmt::getElse() const { return const_cast<Stmt &>(*elseSt); };
const Expr &WhileStmt::getCond() const { return const_cast<Expr &>(*cond); };
const Stmt &WhileStmt::getDo() const { return const_cast<Stmt &>(*doSt); };
Symbol GoToStmt::getIdentifier() const { return identifier; };
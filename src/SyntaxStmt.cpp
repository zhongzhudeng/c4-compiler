#include "SyntaxStmt.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxExternDecl.hpp"

Stmt::Stmt(Locatable loc, Kind kind) : loc(loc), kind(kind) {}
Stmt::~Stmt() {}
const Locatable &Stmt::getLoc() { return loc; }
Stmt::Kind Stmt::getKind() const { return kind; }

IdentifierStmt::IdentifierStmt(Locatable loc, Symbol identifier, StmtPtr stmt)
    : Stmt(loc, Kind::Identifier), identifier(identifier),
      stmt(std::move(stmt)) {}
CompStmt::CompStmt(Locatable loc, std::vector<StmtPtr> blockItemList)
    : Stmt(loc, Kind::Comp), blockItemList(std::move(blockItemList)){};
DeclStmt::DeclStmt(Locatable loc, DeclPtr decl)
    : Stmt(loc, Kind::Decl), decl(std::move(decl)) {}
ExprStmt::ExprStmt(Locatable loc, ExprPtr expr)
    : Stmt(loc, Kind::Expr), expr(std::move(expr)){};
IfElseStmt::IfElseStmt(Locatable loc, ExprPtr cond, StmtPtr then, StmtPtr Else)
    : Stmt(loc, Kind::IfElse), cond(std::move(cond)), then(std::move(then)),
      Else(std::move(Else)){};
WhileStmt::WhileStmt(Locatable loc, ExprPtr cond, StmtPtr doSt)
    : Stmt(loc, Kind::While), cond(std::move(cond)), doSt(std::move(doSt)){};
GoToStmt::GoToStmt(Locatable loc, Symbol identifier)
    : Stmt(loc, Kind::GoTo), identifier(identifier){};
ContinueStmt::ContinueStmt(Locatable loc) : Stmt(loc, Kind::Continue){};
BreakStmt::BreakStmt(Locatable loc) : Stmt(loc, Kind::Break){};
ReturnStmt::ReturnStmt(Locatable loc, ExprPtr expr)
    : Stmt(loc, Kind::Return), expr(std::move(expr)){};

Symbol IdentifierStmt::getIdentifier() const { return identifier; };
const Stmt &IdentifierStmt::getStmt() const {
  return const_cast<Stmt &>(*stmt);
};
const std::vector<std::reference_wrapper<const Stmt>>
CompStmt::getBlockItemList() const {
  std::vector<std::reference_wrapper<const Stmt>> res;
  for (auto &blockItem : blockItemList)
    res.emplace_back(std::cref(*blockItem));
  return res;
};
const class Decl &DeclStmt::getDeclaration() const {
  return const_cast<class Decl &>(*decl);
}
const Expr &ExprStmt::getExpr() const {
  return const_cast<class Expr &>(*expr);
};
bool ExprStmt::isEmpty() const { return expr == nullptr; }
const Expr &IfElseStmt::getCond() const {
  return const_cast<class Expr &>(*cond);
};
const Stmt &IfElseStmt::getThen() const { return const_cast<Stmt &>(*then); };
const Stmt &IfElseStmt::getElse() const { return const_cast<Stmt &>(*Else); };
bool IfElseStmt::isElseEmpty() const { return Else == nullptr; }
const Expr &WhileStmt::getCond() const {
  return const_cast<class Expr &>(*cond);
};
const Stmt &WhileStmt::getDo() const { return const_cast<Stmt &>(*doSt); };
Symbol GoToStmt::getIdentifier() const { return identifier; };
const class Expr &ReturnStmt::getExpr() const {
  return const_cast<class Expr &>(*expr);
}
bool ReturnStmt::isEmpty() const { return expr == nullptr; }
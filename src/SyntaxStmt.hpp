#pragma once
#include "Diagnostic.hpp"
#include "Printer.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include <vector>

class IdentifierStmt final : public Stmt {
  Symbol identifier;
  StmtPtr stmt;
public:
  IdentifierStmt(Locatable loc, Symbol identifier, StmtPtr stmt);
  ~IdentifierStmt() = default;
  void accept(ASTPrinter &visitor) override;
  Symbol getIdentifier() const;
  const Stmt &getStmt() const;
};
class CompStmt final : public Stmt {
  //BlockItemList_opt Vector also Decl
  std::vector<ASTPtr> blockItemList;
public:
  CompStmt(Locatable loc, 
  std::vector<ASTPtr> blockItemList);
  ~CompStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const std::vector<ASTPtr> &getBlockItemList() const;
  bool isComp() const override {
    return true;
  }
};
class ExprStmt final : public Stmt {
  //Expression_opt
  ExprPtr expr;
public:
  ExprStmt(Locatable loc, ExprPtr expr);
  ~ExprStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const Expr &getExpr() const;
};
class IfStmt final : public Stmt {
  ExprPtr cond;
  StmtPtr ifSt;
public:
  IfStmt(Locatable loc, ExprPtr cond, StmtPtr ifSt);
  ~IfStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const Expr &getCond() const;
  const Stmt &getIf() const;
  bool isIf() const override {
    return true;
  }
}; 
class IfElseStmt final : public Stmt {
  ExprPtr cond;
  StmtPtr ifSt;
  StmtPtr elseSt;
public:
  IfElseStmt(Locatable loc, ExprPtr cond, StmtPtr ifSt, StmtPtr elseSt);
  ~IfElseStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const Expr &getCond() const;
  const Stmt &getIf() const;
  const Stmt &getElse() const;
  bool isIf() const override {
    return true;
  }
};
class WhileStmt final : public Stmt {
  ExprPtr cond;
  StmtPtr doSt;
public:
  WhileStmt(Locatable loc, ExprPtr cond, StmtPtr doSt);
  ~WhileStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const Expr &getCond() const;
  const Stmt &getDo() const;
};
class GoToStmt final : public Stmt {
  Symbol identifier;
public:
  GoToStmt(Locatable loc, Symbol identifier);
  ~GoToStmt() = default;
  void accept(ASTPrinter &visitor) override;
  Symbol getIdentifier() const;
};
class ContinueStmt final : public Stmt {
public:
  ContinueStmt(Locatable loc);
  ~ContinueStmt() = default;
  void accept(ASTPrinter &visitor) override;
};
class BreakStmt final : public Stmt {
public:
  BreakStmt(Locatable loc);
  void accept(ASTPrinter &visitor) override;
};

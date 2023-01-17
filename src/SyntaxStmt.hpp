#pragma once
#include "Diagnostic.hpp"
#include "Printer.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include <vector>

class Stmt : public AST {
public:
  virtual ~Stmt() = 0;
  virtual bool isIf() const{
    return false;
  }
  virtual bool isComp() const{
    return false;
  }
};

class IdentifierStmt : public Stmt {
  Symbol identifier;
  StmtPtr stmt;
public:
  IdentifierStmt(Symbol identifier, StmtPtr stmt);
  ~IdentifierStmt() = default;
  void accept(ASTPrinter &visitor) override;
  Symbol getIdentifier() const;
  const Stmt &getStmt() const;
};
class CompStmt : public Stmt {
  //BlockItemList_opt Vector also Decl
  std::vector<StmtPtr> blockItemList;
public:
  CompStmt(
  std::vector<StmtPtr> blockItemList);
  ~CompStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const std::vector<StmtPtr> &getBlockItemList() const;
  bool isComp() const override {
    return true;
  }
};
class ExprStmt : public Stmt {
  //Expression_opt
  ExprPtr expr;
public:
  ExprStmt(ExprPtr expr);
  ~ExprStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const Expr &getExpr() const;
};
class IfStmt : public Stmt {
  ExprPtr cond;
  StmtPtr ifSt;
public:
  IfStmt(ExprPtr cond, StmtPtr ifSt);
  ~IfStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const Expr &getCond() const;
  const Stmt &getIf() const;
  bool isIf() const override {
    return true;
  }
}; 
class IfElseStmt : public Stmt {
  ExprPtr cond;
  StmtPtr ifSt;
  StmtPtr elseSt;
public:
  IfElseStmt(ExprPtr cond, StmtPtr ifSt, StmtPtr elseSt);
  ~IfElseStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const Expr &getCond() const;
  const Stmt &getIf() const;
  const Stmt &getElse() const;
  bool isIf() const override {
    return true;
  }
};
class WhileStmt : public Stmt {
  ExprPtr cond;
  StmtPtr doSt;
public:
  WhileStmt(ExprPtr cond, StmtPtr doSt);
  ~WhileStmt() = default;
  void accept(ASTPrinter &visitor) override;
  const Expr &getCond() const;
  const Stmt &getDo() const;
};
class GoToStmt : public Stmt {
  Symbol identifier;
public:
  GoToStmt(Symbol identifier);
  ~GoToStmt() = default;
  void accept(ASTPrinter &visitor) override;
  Symbol getIdentifier() const;
};
class ContinueStmt : public Stmt {
public:
  ContinueStmt();
  ~ContinueStmt() = default;
  void accept(ASTPrinter &visitor) override;
};
class BreakStmt : public Stmt {
public:
  BreakStmt();
  ~BreakStmt() = default;
  void accept(ASTPrinter &visitor) override;
};

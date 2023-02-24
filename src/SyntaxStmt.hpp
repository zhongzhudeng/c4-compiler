#pragma once
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include <vector>

class Stmt {
public:
  enum Kind {
#define STMT(DERIVED, BASE) DERIVED,
#define ABSTRACT_STMT(STMT)
#define STMT_RANGE(BASE, START, END) first##BASE = START, last##BASE = END,
#define LAST_STMT_RANGE(BASE, START, END) first##BASE = START, last##BASE = END
#include "NodesStmt.def"
  };
private:
  Locatable loc;
  Kind kind;

public:
  Stmt(Locatable loc, Kind kind);
  virtual ~Stmt();
  const Locatable &getLoc();
  Kind getKind() const;
  void print(std::ostream &os, unsigned identation = 0) const;
};

class LabeledStmt final : public Stmt {
  LabelDeclPtr label;
  StmtPtr stmt;

public:
  LabeledStmt(Locatable loc, LabelDeclPtr label, StmtPtr stmt);
  const LabelDecl &getLabelDecl() const;
  const Stmt &getStmt() const;
};
class CompStmt final : public Stmt {
  // BlockItemList_opt Vector also Decl
  std::vector<StmtPtr> blockItemList;

public:
  CompStmt(Locatable loc, std::vector<StmtPtr> blockItemList);
  const std::vector<std::reference_wrapper<const Stmt>>
  getBlockItemList() const;
};

// Wrapper for Declaration in compound statement
class DeclStmt final : public Stmt {
  DeclaratorDeclPtr decl;

public:
  DeclStmt(Locatable loc, DeclaratorDeclPtr decl);
  const class DeclaratorDecl &getDeclaration() const;
};

class ExprStmt final : public Stmt {
  // Expression_opt
  ExprPtr expr;

public:
  ExprStmt(Locatable loc, ExprPtr expr);
  const class Expr &getExpr() const;
  bool isEmpty() const;
};
class IfElseStmt final : public Stmt {
  ExprPtr cond;
  StmtPtr then;
  StmtPtr Else;

public:
  IfElseStmt(Locatable loc, ExprPtr cond, StmtPtr then, StmtPtr Else);
  const class Expr &getCond() const;
  const Stmt &getThen() const;
  const Stmt &getElse() const;
  bool isElseEmpty() const;
};
class WhileStmt final : public Stmt {
  ExprPtr cond;
  StmtPtr doSt;

public:
  WhileStmt(Locatable loc, ExprPtr cond, StmtPtr doSt);
  const class Expr &getCond() const;
  const Stmt &getDo() const;
};
class GoToStmt final : public Stmt {
  const LabelDecl *label_decl;

public:
  GoToStmt(Locatable loc, const LabelDecl *label_decl = nullptr);
  void set_label_decl(const LabelDecl *label_decl);
  const LabelDecl &getLabelDecl() const;
};
class ContinueStmt final : public Stmt {
public:
  ContinueStmt(Locatable loc);
};
class BreakStmt final : public Stmt {
public:
  BreakStmt(Locatable loc);
};
class ReturnStmt final : public Stmt {
  ExprPtr expr;

public:
  ReturnStmt(Locatable loc, ExprPtr expr);
  const class Expr &getExpr() const;
  bool isEmpty() const;
};
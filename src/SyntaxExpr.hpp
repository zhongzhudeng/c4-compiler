#pragma once

#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include <vector>

class Expr {
public:
  enum Kind {
#define EXPR(DERIVED, BASE) DERIVED,
#define ABSTRACT_EXPR(EXPR)
#define EXPR_RANGE(BASE, START, END) first##BASE = START, last##BASE = END,
#define LAST_EXPR_RANGE(BASE, START, END) first##BASE = START, last##BASE = END
#include "NodesExpr.def"
  };
private:
  Locatable loc;
  Kind kind;

public:
  Expr(Locatable loc, Kind kind);
  virtual ~Expr(){}
  const Locatable &getLoc();
  Kind getKind() const;
  void print(std::ostream &os) const;
};

class IdentifierExpr final : public Expr {
  Symbol identifier;

public:
  IdentifierExpr(Locatable loc, Symbol identifier);
  Symbol getIdentifier() const;
};
class ConstantExpr final : public Expr {
  Symbol val;

public:
  ConstantExpr(Locatable loc, Symbol val);
  Symbol getConstant() const;
};
class StringLiteralExpr final : public Expr {
  Symbol val;

public:
  StringLiteralExpr(Locatable loc, Symbol val);
  Symbol getStringLiteral() const;
};

class ParenExpr final : public Expr {
  ExprPtr expr;

public:
  ParenExpr(Locatable loc, ExprPtr expr);
  const Expr &getExpr() const;
};

class ConditionalExpr final : public Expr {
  ExprPtr first, second, third;

public:
  ConditionalExpr(Locatable loc, ExprPtr first, ExprPtr second, ExprPtr third);
  const Expr &getFirst() const;
  const Expr &getSecond() const;
  const Expr &getThird() const;
};

class BinaryExpr final : public Expr {
  Symbol BinaryOp;
  ExprPtr LHS, RHS;

public:
  BinaryExpr(Locatable loc, Symbol Op, ExprPtr LHS, ExprPtr RHS);
  const Expr &getLHS() const;
  Symbol getOperator() const;
  const Expr &getRhS() const;
};

class UnaryExpr final : public Expr {
  Symbol UnaryOp;
  ExprPtr expr;

public:
  UnaryExpr(Locatable loc, Symbol UnaryOp, ExprPtr expr);
  Symbol getOperator() const;
  const Expr &getExpr() const;
};

class SizeofExpr final : public Expr {
  ExprPtr expr;

public:
  SizeofExpr(Locatable loc, ExprPtr expr);
  const Expr &getExpr() const;
};
class SizeofTypeExpr final : public Expr {
  Symbol type_name;

public:
  SizeofTypeExpr(Locatable loc, Symbol type_name);
  Symbol getTypeName() const;
};

class ArraySubscriptExpr final : public Expr {
  ExprPtr expr, squaredExpr;

public:
  ArraySubscriptExpr(Locatable loc, ExprPtr expr, ExprPtr squaredExpr);
  const Expr &getExpr() const;
  const Expr &getSquaredExpr() const;
};
class FunctionCallExpr final : public Expr {
  ExprPtr callee;
  std::vector<ExprPtr> args;

public:
  FunctionCallExpr(Locatable loc, ExprPtr callee, std::vector<ExprPtr> args);
  const Expr &getCallee() const;
  const std::vector<ExprPtr> &getArgs() const;
};

class MemberExpr final : public Expr {
  ExprPtr expr, member;
  Symbol acccesOp;

public:
  MemberExpr(Locatable loc, ExprPtr expr, ExprPtr member, Symbol acccesOp);
  const Expr &getExpr() const;
  Symbol getAccessOp() const;
  const Expr &getMember() const;
};
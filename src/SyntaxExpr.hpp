#pragma once

#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include <vector>

class Expr : public AST {
public:
  Expr(Locatable loc);
  virtual ~Expr() = 0;
};

class IdentifierExpr final : public Expr {
  Symbol identifier;

public:
  IdentifierExpr(Locatable loc, Symbol identifier);
  void accept(ASTPrinter &visitor) override;
  Symbol getIdentifier() const;
};
class ConstantExpr final : public Expr {
  Symbol val;

public:
  ConstantExpr(Locatable loc, Symbol val);
  void accept(ASTPrinter &visitor) override;
  Symbol getConstant() const;
};
class StringLiteralExpr final : public Expr {
  Symbol val;

public:
  StringLiteralExpr(Locatable loc, Symbol val);
  void accept(ASTPrinter &visitor) override;
  Symbol getStringLiteral() const;
};

class ParenExpr final : public Expr {
  ExprPtr expr;

public:
  ParenExpr(Locatable loc, ExprPtr expr);
  void accept(ASTPrinter &visitor) override;
  const Expr &getExpr() const;
};

class AssignmentExpr final : public Expr {
  ExprPtr LHS, RHS;

public:
  AssignmentExpr(Locatable loc, ExprPtr LHS, ExprPtr RHS);
  void accept(ASTPrinter &visitor) override;
  const Expr &getLHS() const;
  const Expr &getRHS() const;
};

class ConditionalExpr final : public Expr {
  ExprPtr first, second, third;

public:
  ConditionalExpr(Locatable loc, ExprPtr first, ExprPtr second, ExprPtr third);
  void accept(ASTPrinter &visitor) override;
  const Expr &getFirst() const;
  const Expr &getSecond() const;
  const Expr &getThird() const;
};

class BinaryExpr final : public Expr {
  Symbol BinaryOp;
  ExprPtr LHS, RHS;

public:
  BinaryExpr(Locatable loc, Symbol Op, ExprPtr LHS, ExprPtr RHS);
  void accept(ASTPrinter &visitor) override;
  const Expr &getLHS() const;
  Symbol getOperator() const;
  const Expr &getRhS() const;
};

class UnaryExpr final : public Expr {
  Symbol UnaryOp;
  ExprPtr expr;

public:
  UnaryExpr(Locatable loc, Symbol UnaryOp, ExprPtr expr);
  void accept(ASTPrinter &visitor) override;
  Symbol getOperator() const;
  const Expr &getExpr() const;
};

class SizeofExpr final : public Expr {
  ExprPtr expr;

public:
  SizeofExpr(Locatable loc, ExprPtr expr);
  void accept(ASTPrinter &visitor) override;
  const Expr &getExpr() const;
};
class SizeofTypeExpr final : public Expr {
  Symbol type_name;

public:
  SizeofTypeExpr(Locatable loc, Symbol type_name);
  void accept(ASTPrinter &visitor) override;
  Symbol getTypeName() const;
};
class CastExpr final : public Expr {
  Symbol type_name;
  ExprPtr expr;

public:
  CastExpr(Locatable loc, Symbol type_name, ExprPtr expr);
  void accept(ASTPrinter &visitor) override;
  Symbol getTypeName() const;
  const Expr &getExpr() const;
};
class ArraySubscriptExpr final : public Expr {
  ExprPtr expr, squaredExpr;

public:
  ArraySubscriptExpr(Locatable loc, ExprPtr expr, ExprPtr squaredExpr);
  void accept(ASTPrinter &visitor) override;
  const Expr &getExpr() const;
  const Expr &getSquaredExpr() const;
};
class FunctionCallExpr final : public Expr {
  ExprPtr callee;
  std::vector<ExprPtr> args;

public:
  FunctionCallExpr(Locatable loc, ExprPtr callee, std::vector<ExprPtr> args);
  void accept(ASTPrinter &visitor) override;
  const Expr &getCallee() const;
  const std::vector<ExprPtr> &getArgs() const;
};

class MemberExpr final : public Expr {
  ExprPtr expr, member;
  Symbol acccesOp;

public:
  MemberExpr(Locatable loc, ExprPtr expr, ExprPtr member, Symbol acccesOp);
  void accept(ASTPrinter &visitor) override;
  const Expr &getExpr() const;
  Symbol getAccessOp() const;
  const Expr &getMember() const;
};
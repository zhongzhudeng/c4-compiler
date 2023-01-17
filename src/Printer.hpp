#pragma once
#include "Syntax.hpp"
#include <ostream>

std::ostream &operator<<(std::ostream &out, const AST &ast);

class ASTPrinter {
  std::ostream &os;

public:
  ASTPrinter(std::ostream &os);
  void Visit(const AST &ast);
  void Visit(const IdentifierExpr &expr);
  void Visit(const ConstantExpr &expr);
  void Visit(const StringLiteralExpr &expr);
  void Visit(const ParenExpr &expr);
  void Visit(const AssignmentExpr &expr);
  void Visit(const ConditionalExpr &expr);
  void Visit(const BinaryExpr &expr);
  void Visit(const UnaryExpr &expr);
  void Visit(const SizeofExpr &expr);
  void Visit(const SizeofTypeExpr &expr);
  void Visit(const ArraySubscriptExpr &expr);
  void Visit(const FunctionCallExpr &expr);
  void Visit(const MemberExpr &expr);
};
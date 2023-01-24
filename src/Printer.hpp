#pragma once
#include "Syntax.hpp"
#include "SyntaxExternalDecl.hpp"
#include <ostream>

std::ostream &operator<<(std::ostream &out, const AST &ast);

class ASTPrinter {
  std::ostream &os;
  int indent = 0;

public:
  ASTPrinter(std::ostream &os);

  void increaseIndent();
  void decreaseIndent();
  void nextLine();

  void Visit(const AST &ast);

  void Visit(const TranslationUnit &tu);
  void Visit(const ExternalDecl &externalDecl);

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

  void Visit(const Stmt &stmt);
  void Visit(const IdentifierStmt &stmt);
  void Visit(const CompStmt &stmt);
  void Visit(const ExprStmt &stmt);
  void Visit(const IfStmt &stmt);
  void Visit(const IfElseStmt &stmt);
  void Visit(const WhileStmt &stmt);
  void Visit(const GoToStmt &stmt);
  void Visit(const ContinueStmt &stmt);
  void Visit(const BreakStmt &stmt);
};
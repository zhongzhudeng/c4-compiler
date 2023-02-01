#include "NodeVisitor.hpp"
#include "Scope.hpp"
#include "SyntaxStmt.hpp"

#include <map>

// namespace {
//  What is the type of Function Definitions
//  The function table is a multimap, mapping function names to overloaded
//  definitions.
// using FuncTable = std::multimap<std::string, const FuncDef*>;
// }

class ExprTypeChecker : public ExprVisitor<ExprTypeChecker> {

public:
  void VisitIdentifierExpr(const IdentifierExpr &expr) {}
  void VisitConstantExpr(const ConstantExpr &expr) {}
  void VisitStringLiteralExpr(const StringLiteralExpr &expr) {}
  void VisitParenExpr(const ParenExpr &expr) {}
  void VisitConditionalExpr(const ConditionalExpr &expr) {}
  void VisitBinaryExpr(const BinaryExpr &expr) {}
  void VisitUnaryExpr(const UnaryExpr &expr) {}
  void VisitSizeofExpr(const SizeofExpr &expr) {}
  void VisitSizeofTypeExpr(const SizeofTypeExpr &expr) {}
  void VisitArraySubscriptExpr(const ArraySubscriptExpr &expr) {}
  void VisitFunctionCallExpr(const FunctionCallExpr &expr) {}
  void VisitMemberExpr(const MemberExpr &expr) {}
  ExprTypeChecker() {}
};

class StmtPrinter : public StmtVisitor<StmtPrinter> {

public:
  // stmtTypeChecker(Scope* scope, const FuncTable& funcTable, const FuncDef&
  // enclosingFunction)
  //     : m_scope(scope), m_funcTable(funcTable),
  //     m_enclosingFunction(enclosingFunction){}
  void VisitIdentifierStmt(const LabeledStmt &stmt) {}
  void VisitCompStmt(const CompStmt &stmt) {}
  void VisitDeclStmt(const DeclStmt &stmt) {}
  void VisitExprStmt(const ExprStmt &stmt) {}
  void VisitIfElseStmt(const IfElseStmt &stmt) {}
  void VisitWhileStmt(const WhileStmt &stmt) {}
  void VisitGoToStmt(const GoToStmt &stmt) {}
  void VisitContinueStmt(const ContinueStmt &stmt) {}
  void VisitBreakStmt(const BreakStmt &stmt) {}

  StmtPrinter() {}
};
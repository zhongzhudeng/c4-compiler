#pragma once

#include "AST.hpp"
#include "SemanticModel.hpp"

#include <ostream>
#include <string>

/// Pretty printer for the syntax tree.  Formatting follows the reference
/// file from project task D: tabs for indentation, fully parenthesized
/// expressions and declarators, labels at column zero, and one blank line
/// between file-scope declarations.
class Printer {
public:
  Printer(const AST &ast, const SemanticModel &model, std::ostream &out);

  void print();

private:
  const AST &ast_;
  std::ostream &out_;

  void indent(int level);

  void printDecl(DeclId id, int level);
  void printStmt(StmtId id, int level);
  void printTypeSpec(TypeSpecId id, int level);

  void printCompoundBody(StmtId id, int level);
  void printCompoundInline(StmtId id, int level);
  void printControlledBody(StmtId id, int level);
  void printIf(StmtId id, int level, bool leadingIndent);

  std::string typeSpecFlat(TypeSpecId id) const;
  std::string declaratorString(DeclaratorId id) const;
  std::string parameterString(DeclId id) const;
  std::string fieldFlat(DeclId id) const;
  std::string expressionString(ExprId id) const;

  static const char *binaryOperator(BinaryExpr::Op op);
};

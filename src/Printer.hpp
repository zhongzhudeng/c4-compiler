#pragma once

#include "AST.hpp"

#include <ostream>

/// Pretty printer for the syntax tree.  Formatting follows the reference
/// file from project task D: tabs for indentation, fully parenthesized
/// expressions and declarators, labels at column zero, and one blank line
/// between file-scope declarations.
///
/// Rendering writes directly to the output stream; the printer owns no
/// intermediate text and depends only on the AST.
class Printer {
public:
  Printer(const AST &ast, std::ostream &out);

  void print();

private:
  const AST &ast_;
  std::ostream &out_;

  void indent(int level);

  void printDecl(DeclId id, int level);
  void printStmt(StmtId id, int level);
  void printTypeSpec(TypeSpecId id, int level);
  void printFlatTypeSpec(TypeSpecId id);
  void printFlatField(DeclId id);
  void printParameter(DeclId id);
  void printParameterList(const FunctionDeclarator &function);
  void printExpr(ExprId id);
  void printDeclarator(DeclaratorId id);

  void printTypeAndDeclarator(TypeSpecId spec, DeclaratorId declarator,
                              int level);
  void printFlatTypeAndDeclarator(TypeSpecId spec, DeclaratorId declarator);

  bool isEmptyDeclarator(DeclaratorId id) const;
  bool isBarePointerDeclarator(DeclaratorId id) const;

  void printCompound(StmtId id, int level, bool leadingIndent,
                     bool trailingNewline);
  void printCompoundBody(StmtId id, int level);
  void printCompoundInline(StmtId id, int level);
  void printControlledBody(StmtId id, int level);
  void printIf(StmtId id, int level, bool leadingIndent);

  static const char *binaryOperator(BinaryExpr::Op op);
  static const char *unaryOperator(UnaryExpr::Op op);
};

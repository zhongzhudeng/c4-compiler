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
    Printer(const AST& ast, std::ostream& out);

    void print();

private:
    const AST& ast_;
    std::ostream& out_;

    void indent(int level);

    void print_decl(DeclId id, int level);
    void print_stmt(StmtId id, int level);
    void print_type_spec(TypeSpecId id, int level);
    void print_flat_type_spec(TypeSpecId id);
    void print_flat_field(DeclId id);
    void print_parameter(DeclId id);
    void print_parameter_list(const FunctionDeclarator& function);
    void print_expr(ExprId id);
    void print_declarator(DeclaratorId id);

    void print_type_and_declarator(TypeSpecId spec, DeclaratorId declarator, int level);
    void print_flat_type_and_declarator(TypeSpecId spec, DeclaratorId declarator);

    bool is_empty_declarator(DeclaratorId id) const;
    bool is_bare_pointer_declarator(DeclaratorId id) const;

    void print_compound(StmtId id, int level, bool leading_indent, bool trailing_newline);
    void print_compound_body(StmtId id, int level);
    void print_compound_inline(StmtId id, int level);
    void print_controlled_body(StmtId id, int level);
    void print_if(StmtId id, int level, bool leading_indent);

    static const char* binary_operator(BinaryExpr::Op op);
    static const char* unary_operator(UnaryExpr::Op op);
};

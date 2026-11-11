#pragma once

#include "AST.hpp"
#include "Support.hpp"
#include "Token.hpp"

#include <cstddef>
#include <optional>
#include <vector>

/// Recursive-descent parser for the restricted C4 grammar.  It performs
/// syntax analysis only and fills the ID-based AST; all names and types are
/// resolved later by Sema.
class Parser {
public:
    Parser(AST& ast, std::vector<Token> tokens, Diagnostics& diagnostics);

    void parse_translation_unit();

private:
    AST& ast_;
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    Diagnostics& diagnostics_;

    const Token& current() const;
    const Token& peek_token(size_t offset) const;
    TokenKind peek_kind(size_t offset) const;
    SourceLoc loc() const;
    bool at_end() const;
    bool check(TokenKind kind) const;
    bool accept(TokenKind kind);
    void expect(TokenKind kind, const char* what);
    void advance();
    [[noreturn]] void error(SourceLoc location, const std::string& message) const;

    bool is_type_specifier_start() const;
    bool is_declarator_start() const;
    bool is_type_name_ahead() const;
    bool is_parameter_list_ahead() const;

    TypeSpecId parse_type_spec();
    DeclaratorId parse_declarator(bool allow_name, bool allow_empty);
    FunctionDeclarator::Parameters parse_parameter_list();
    DeclId parse_parameter_declaration();
    std::vector<DeclId> parse_field_declaration();

    DeclId parse_external_declaration();
    DeclId parse_local_declaration();

    StmtId parse_statement();
    StmtId parse_compound_statement();

    ExprId parse_expression();
    ExprId parse_conditional();
    ExprId parse_binary(int min_precedence);
    ExprId parse_unary();
    ExprId parse_postfix();
    ExprId parse_primary();

    bool is_assignment_lhs(ExprId id) const;

    /// A binary operator together with its precedence.  Returning both at once
    /// keeps the operator and its precedence from drifting apart.
    struct BinaryOperator {
        BinaryExpr::Op op;
        int precedence;
    };
    static std::optional<BinaryOperator> binary_precedence(TokenKind kind);
};

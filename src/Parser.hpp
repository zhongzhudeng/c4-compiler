#pragma once

#include "AST.hpp"
#include "Support.hpp"
#include "Token.hpp"

#include <cstddef>
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
    std::vector<DeclId> parse_parameter_list(SourceLoc lparen, bool& explicit_void);
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
    static int binary_precedence(TokenKind kind, BinaryExpr::Op& op);
};

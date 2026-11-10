#pragma once

#include "Support.hpp"
#include "SymbolInternalizer.hpp"
#include "Token.hpp"

#include <cstddef>
#include <string>
#include <vector>

/// Hand-written lexer for the C4 subset.  It implements the maximal-munch
/// rule, ISO-8859-1 byte positions, CR/LF/CRLF line endings, and the
/// restricted constant/string rules from the assignment.
class Lexer {
public:
    Lexer(std::string filename, std::string source, SymbolInternalizer& symbols);

    Lexer(const Lexer&) = delete;
    Lexer& operator=(const Lexer&) = delete;

    /// Returns the complete token stream, including the end-of-input token.
    std::vector<Token> tokenize();

private:
    std::string source_;
    size_t pos_ = 0;
    uint32_t line_ = 1;
    uint32_t column_ = 1;
    SymbolInternalizer& symbols_;
    Diagnostics diagnostics_;
    Symbol filename_symbol_;

    int peek(size_t offset = 0) const;
    bool eof() const;
    SourceLoc here() const {
        return SourceLoc{line_, column_};
    }
    void consume();

    void skip_whitespace_and_comments();
    Token lex_token();
    TokenKind lex_number();
    TokenKind lex_character_constant();
    TokenKind lex_string_literal();
    TokenKind lex_punctuator();

    Token make_token(TokenKind kind, SourceLoc start, size_t begin) const;
    [[noreturn]] void fail(SourceLoc loc, const std::string& message) const;

    static bool is_ident_start(int c);
    static bool is_ident_continue(int c);
    static bool is_digit(int c);
    static bool is_simple_escape(int c);
};

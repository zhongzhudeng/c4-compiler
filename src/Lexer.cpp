#include "Lexer.hpp"

#include <string_view>
#include <utility>

namespace {

struct Punctuator {
    const char* text;
    TokenKind kind;
};

// Longest spellings must be tried first.  This is the complete C11
// punctuator set; unsupported operators are still lexed and are rejected by
// the parser later.
constexpr Punctuator K_PUNCTUATORS[] = {
    {"%:%:", TkPercentcolonpercentcolon},
    {"<<=", TkLesslessequal},
    {">>=", TkGreatergreaterequal},
    {"...", TkEllipsis},
    {"##", TkHashhash},
    {"->", TkArrow},
    {"++", TkPlusplus},
    {"--", TkMinusminus},
    {"<<", TkLessless},
    {">>", TkGreatergreater},
    {"<=", TkLessequal},
    {">=", TkGreaterequal},
    {"==", TkEqualequal},
    {"!=", TkExclaimequal},
    {"&&", TkAmpamp},
    {"||", TkPipepipe},
    {"*=", TkStarequal},
    {"/=", TkSlashequal},
    {"%=", TkPercentequal},
    {"+=", TkPlusequal},
    {"-=", TkMinusequal},
    {"&=", TkAmpequal},
    {"^=", TkCaretequal},
    {"|=", TkPipeequal},
    {"<:", TkLesscolon},
    {":>", TkColongreater},
    {"<%", TkLesspercent},
    {"%>", TkPercentgreater},
    {"%:", TkPercentcolon},
    {"[", TkLsquare},
    {"]", TkRsquare},
    {"(", TkLparen},
    {")", TkRparen},
    {"{", TkLbrace},
    {"}", TkRbrace},
    {".", TkPeriod},
    {"&", TkAmp},
    {"*", TkStar},
    {"+", TkPlus},
    {"-", TkMinus},
    {"~", TkTild},
    {"!", TkExclaim},
    {"/", TkSlash},
    {"%", TkPercent},
    {"<", TkLess},
    {">", TkGreater},
    {"^", TkCaret},
    {"|", TkPipe},
    {"?", TkQuestion},
    {":", TkColon},
    {";", TkSemicolon},
    {"=", TkEqual},
    {",", TkComma},
    {"#", TkHash},
};

} // namespace

Lexer::Lexer(std::string filename, std::string source, SymbolInternalizer& symbols)
    : source_(std::move(source)), symbols_(symbols), diagnostics_(std::move(filename)) {
    filename_symbol_ = symbols_.internalize(diagnostics_.filename());
}

int Lexer::peek(size_t offset) const {
    if (pos_ + offset >= source_.size())
        return -1;
    return static_cast<unsigned char>(source_[pos_ + offset]);
}

bool Lexer::eof() const {
    return pos_ >= source_.size();
}

void Lexer::consume() {
    if (eof())
        return;
    int c = peek();
    ++pos_;
    if (c == '\r') {
        // CRLF is one newline; a lone CR is also one newline.
        if (peek() == '\n')
            ++pos_;
        ++line_;
        column_ = 1;
    } else if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
}

bool Lexer::is_ident_start(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool Lexer::is_ident_continue(int c) {
    return is_ident_start(c) || is_digit(c);
}

bool Lexer::is_digit(int c) {
    return c >= '0' && c <= '9';
}

bool Lexer::is_simple_escape(int c) {
    switch (c) {
        case '\'':
        case '"':
        case '?':
        case '\\':
        case 'a':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case 'v':
            return true;
        default:
            return false;
    }
}

void Lexer::skip_whitespace_and_comments() {
    for (;;) {
        int c = peek();
        if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n') {
            consume();
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            consume();
            consume();
            while (!eof() && peek() != '\n' && peek() != '\r')
                consume();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            SourceLoc start = here();
            consume();
            consume();
            bool closed = false;
            while (!eof()) {
                if (peek() == '*' && peek(1) == '/') {
                    consume();
                    consume();
                    closed = true;
                    break;
                }
                consume();
            }
            if (!closed)
                fail(start, "unterminated comment");
            continue;
        }
        return;
    }
}

void Lexer::fail(SourceLoc loc, const std::string& message) const {
    diagnostics_.error(loc, message);
}

Token Lexer::make_token(TokenKind kind, SourceLoc start, size_t begin) const {
    std::string_view spelling(source_.data() + begin, pos_ - begin);
    return Token(start, kind, symbols_.internalize(spelling), filename_symbol_);
}

TokenKind Lexer::lex_number() {
    if (peek() == '0') {
        consume();
        if (is_digit(peek()))
            fail(here(), "octal constants are not supported");
        if (is_ident_start(peek()) || peek() == '.')
            fail(here(), "invalid suffix on integer constant");
        return TkConstant;
    }
    while (is_digit(peek()))
        consume();
    if (is_ident_start(peek()) || peek() == '.')
        fail(here(), "invalid suffix on integer constant");
    return TkConstant;
}

TokenKind Lexer::lex_character_constant() {
    SourceLoc start = here();
    consume(); // opening quote
    if (eof() || peek() == '\n' || peek() == '\r')
        fail(start, "missing terminating ' character");
    if (peek() == '\'')
        fail(here(), "empty character constant");
    if (peek() == '\\') {
        SourceLoc escape = here();
        consume();
        if (eof() || peek() == '\n' || peek() == '\r')
            fail(start, "missing terminating ' character");
        if (!is_simple_escape(peek()))
            fail(escape, "invalid escape sequence");
        consume();
    } else {
        if (peek() == 0)
            fail(here(), "invalid character in character constant");
        consume();
    }
    if (eof() || peek() == '\n' || peek() == '\r')
        fail(start, "missing terminating ' character");
    if (peek() != '\'')
        fail(here(), "multi-character character constant");
    consume();
    return TkConstant;
}

TokenKind Lexer::lex_string_literal() {
    SourceLoc start = here();
    consume(); // opening quote
    while (!eof()) {
        int c = peek();
        if (c == '"') {
            consume();
            return TkStringLiteral;
        }
        if (c == '\n' || c == '\r')
            fail(start, "missing terminating \" character");
        if (c == '\\') {
            SourceLoc escape = here();
            consume();
            if (eof() || peek() == '\n' || peek() == '\r')
                fail(start, "missing terminating \" character");
            if (!is_simple_escape(peek()))
                fail(escape, "invalid escape sequence");
            consume();
            continue;
        }
        if (c == 0)
            fail(here(), "invalid character in string literal");
        consume();
    }
    fail(start, "missing terminating \" character");
}

TokenKind Lexer::lex_punctuator() {
    for (const Punctuator& p : K_PUNCTUATORS) {
        size_t length = std::char_traits<char>::length(p.text);
        if (pos_ + length > source_.size())
            continue;
        if (source_.compare(pos_, length, p.text) != 0)
            continue;
        for (size_t i = 0; i < length; ++i)
            consume();
        return p.kind;
    }
    int c = peek();
    std::string message = "invalid character";
    if (c >= 32 && c < 127) {
        message += " '";
        message += static_cast<char>(c);
        message += '\'';
    } else {
        message += " (byte ";
        message += std::to_string(static_cast<unsigned char>(c));
        message += ')';
    }
    fail(here(), message);
}

Token Lexer::lex_token() {
    SourceLoc start = here();
    size_t begin = pos_;
    int c = peek();
    TokenKind kind;
    if (is_ident_start(c)) {
        while (is_ident_continue(peek()))
            consume();
        std::string_view text(source_.data() + begin, pos_ - begin);
#define KEYWORD_ACTION(KD, STR)                                                                    \
    if (text == STR)                                                                               \
        return make_token(KD, start, begin);
#include "KeyWords.def"
#undef KEYWORD_ACTION
        kind = TkIdentifier;
    } else if (is_digit(c)) {
        kind = lex_number();
    } else if (c == '\'') {
        kind = lex_character_constant();
    } else if (c == '"') {
        kind = lex_string_literal();
    } else {
        kind = lex_punctuator();
    }
    return make_token(kind, start, begin);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    for (;;) {
        skip_whitespace_and_comments();
        SourceLoc start = here();
        if (eof()) {
            tokens.push_back(Token(start, TkEoi, symbols_.internalize(""), filename_symbol_));
            return tokens;
        }
        tokens.push_back(lex_token());
    }
}

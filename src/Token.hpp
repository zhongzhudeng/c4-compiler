#pragma once

#include "Support.hpp"
#include "SymbolInternalizer.hpp"

#include <ostream>

#define KIND_ACTION(KD, STR) KD,

/// Token kinds use the names of the C11 token non-terminals where possible.
/// The enum is intentionally unscoped so the TokenKinds.def tables stay
/// readable.
enum TokenKind {
#include "TokenKinds.def"
};

#undef KIND_ACTION

const char* token_kind_name(TokenKind kind);

/// Digraphs are lexed and spelled exactly as written, but the parser and
/// printer treat them as their primary punctuator.
TokenKind canonical_token_kind(TokenKind kind);

/// One lexed token.  text and file_name are interned Symbols, so copying a
/// token does not copy spelling bytes.  Tokens are only produced by the lexer;
/// there is no empty-token state.
struct Token {
    TokenKind kind;
    Symbol text;
    SourceLoc loc;
    // Interned; the token stream does not own a copy of the filename.
    Symbol file_name;

    Token(SourceLoc loc, TokenKind kind, Symbol text, Symbol filename)
        : kind(kind), text(text), loc(loc), file_name(filename) {}
};

std::ostream& operator<<(std::ostream& stream, const Token& tok);

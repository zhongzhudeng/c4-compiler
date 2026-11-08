#pragma once

#include "Support.hpp"
#include "SymbolInternalizer.hpp"

#include <ostream>
#include <string>

#define KIND_ACTION(KD, STR) KD,

/// Token kinds use the names of the C11 token non-terminals where possible.
/// The enum is intentionally unscoped so the existing TokenKinds.def tables
/// stay readable.
enum TokenKind {
#include "TokenKinds.def"
};

#undef KIND_ACTION

const char *tokenKindName(TokenKind kind);

/// Digraphs are lexed and spelled exactly as written, but the parser and
/// printer treat them as their primary punctuator.
TokenKind canonicalTokenKind(TokenKind kind);

struct Token {
  TokenKind Kind = TK_EOI;
  Symbol Text = nullptr;
  SourceLoc Loc;
  std::string FileName;

  Token() = default;
  Token(SourceLoc loc, TokenKind kind, Symbol text, std::string filename)
      : Kind(kind), Text(text), Loc(loc), FileName(std::move(filename)) {}

  bool is(TokenKind kind) const { return Kind == kind; }
  bool isCanonical(TokenKind kind) const {
    return canonicalTokenKind(Kind) == kind;
  }
};

std::ostream &operator<<(std::ostream &stream, const Token &tok);

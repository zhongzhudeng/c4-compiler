#pragma once

#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"

#define KIND_ACTION(KD, STR) KD,

enum TokenKind {
#include "TokenKinds.def"
};

#undef KIND_ACTION

struct Token : public Locatable {
public:
  TokenKind Kind;
  Symbol Text;

  Token(Locatable &loc, TokenKind kind, Symbol text);

  friend std::ostream &operator<<(std::ostream &stream, const Token &tok);
};

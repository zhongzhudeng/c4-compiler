#include "Token.hpp"

const char *tokenKindName(TokenKind kind) {
  switch (kind) {
#define KIND_ACTION(KD, STR)                                                   \
  case KD:                                                                     \
    return STR;
#include "TokenKinds.def"
  }
  return "unknown";
}

TokenKind canonicalTokenKind(TokenKind kind) {
  switch (kind) {
  case TK_LESSCOLON:
    return TK_LSQUARE;
  case TK_COLONGREATER:
    return TK_RSQUARE;
  case TK_LESSPERCENT:
    return TK_LBRACE;
  case TK_PERCENTGREATER:
    return TK_RBRACE;
  case TK_PERCENTCOLON:
    return TK_HASH;
  case TK_PERCENTCOLONPERCENTCOLON:
    return TK_HASHHASH;
  default:
    return kind;
  }
}

std::ostream &operator<<(std::ostream &stream, const Token &tok) {
  if (tok.FileName)
    stream << tok.FileName.str();
  stream << ':' << tok.Loc.line << ':' << tok.Loc.column << ": "
         << tokenKindName(tok.Kind) << ' ';
  if (tok.Text)
    stream << tok.Text.str();
  return stream;
}

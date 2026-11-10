#include "Token.hpp"

const char* token_kind_name(TokenKind kind) {
    switch (kind) {
#define KIND_ACTION(KD, STR)                                                                       \
    case KD:                                                                                       \
        return STR;
#include "TokenKinds.def"
    }
    return "unknown";
}

TokenKind canonical_token_kind(TokenKind kind) {
    switch (kind) {
        case TkLesscolon:
            return TkLsquare;
        case TkColongreater:
            return TkRsquare;
        case TkLesspercent:
            return TkLbrace;
        case TkPercentgreater:
            return TkRbrace;
        case TkPercentcolon:
            return TkHash;
        case TkPercentcolonpercentcolon:
            return TkHashhash;
        default:
            return kind;
    }
}

std::ostream& operator<<(std::ostream& stream, const Token& tok) {
    if (tok.file_name)
        stream << tok.file_name.str();
    stream << ':' << tok.loc.line << ':' << tok.loc.column << ": " << token_kind_name(tok.kind)
           << ' ';
    if (tok.text)
        stream << tok.text.str();
    return stream;
}

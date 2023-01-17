#include "ParserImpl.hpp"
#include "SyntaxExpr.hpp"
#include <utility>

Parser::impl::impl(std::vector<Token> token_stream)
    : token_stream(std::move(token_stream)) {
  cursor = this->token_stream.begin();
  end = this->token_stream.cend();
}

ASTPtr Parser::impl::parse() { return parseExpr(); }

const Locatable &Parser::impl::location(void) { return *cursor; }
void Parser::impl::bump(void) { cursor++; }
bool Parser::impl::check(TokenKind tk) { return cursor->Kind == tk; }
bool Parser::impl::accept(TokenKind tk) {
  if (check(tk)) {
    bump();
    return true;
  }
  return false;
}
void Parser::impl::expect(TokenKind tk, const char *txt) {
  check(tk) ? bump() : errorloc(location(), "Expect ", txt);
}
TokenKind Parser::impl::kind() { return cursor->Kind; }
TokenKind Parser::impl::kind_first() { return (cursor + 1)->Kind; }

bool Parser::impl::is_end() { return cursor == end; }

bool Parser::impl::is_paren_type_name() {
  return kind() == TK_LPAREN and
         (kind_first() == TK_VOID || kind_first() == TK_CHAR ||
          kind_first() == TK_INT) and
         (cursor + 2)->Kind == TK_RPAREN;
}
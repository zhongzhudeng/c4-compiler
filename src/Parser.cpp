#include "Parser.hpp"
#include "Diagnostic.hpp"
#include "Sema.hpp"
#include "Syntax.hpp"
#include <functional>
#include <memory>

#include "Token.hpp"

void Parser::parse(std::function<void(DeclPtr, bool)> f) {
  DeclPtr ast = nullptr;
  do {
    ast = parseExternDecl();
    f(std::move(ast), is_end());
  } while (!is_end());
}

Parser::Parser(std::vector<Token> token_stream, Sema &Action)
    : token_stream(std::move(token_stream)), Action(Action) {
  cursor = this->token_stream.begin();
  end = this->token_stream.cend();
}
Parser::~Parser() {}

const Locatable &Parser::location(void) { return *cursor; }
void Parser::bump(void) { cursor++; }
bool Parser::check(TokenKind tk) { return cursor->Kind == tk; }
bool Parser::accept(TokenKind tk) {
  if (check(tk)) {
    bump();
    return true;
  }
  return false;
}
void Parser::expect(TokenKind tk, const char *txt) {
  check(tk) ? bump() : errorloc(location(), "Expect ", txt);
}
TokenKind Parser::kind() { return cursor->Kind; }
TokenKind Parser::kind_first() { return (cursor + 1)->Kind; }

bool Parser::is_end() { return cursor == end; }

bool Parser::is_paren_type_name() {
  return kind() == TK_LPAREN and
         (kind_first() == TK_VOID || kind_first() == TK_CHAR ||
          kind_first() == TK_INT) and
         (cursor + 2)->Kind == TK_RPAREN;
}
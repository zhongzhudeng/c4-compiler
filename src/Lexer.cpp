#include "Lexer.hpp"

#include <cstdlib>
#include <fstream>
#include <optional>
#include <string_view>

#include "Diagnostic.hpp"
#include "Token.hpp"

Lexer::Lexer(const char * file_name) {
  line = 1;
  column = 1;
  std::ifstream ifs(file_name);
  input = std::string(std::istreambuf_iterator<char>{ifs}, {});
  len_remaining = input.length();
  chars = std::string_view(input).begin();
  end = std::string_view(input).cend();
  this->file_name = file_name;
}

Lexer::~Lexer() {}

void Lexer::tokenize() {
  for (;;) {
    auto tok = advance_token();
    if (tok.Kind == TokenKind::TK_EOI) {
      break;
    }
    if (first() == eof_char) {
      Locatable location(file_name, line, column);
      errorloc(location, "Unexpected EOF\n");
      break;
    }
    std::cout << tok << "\n";
  }
}

std::queue<Token *> Lexer::lex() {
  std::queue<Token *> tokens;

  for (;;) {
    auto tok = advance_token();
    if (tok.Kind == TokenKind::TK_EOI) {
      return tokens;
    }
    if (first() == eof_char) {
      Locatable location(file_name, line, column);
      errorloc(location, "Unexpected EOF\n");
      return tokens;
    }
    tokens.push(&tok);
  };
}

Token Lexer::advance_token() {
  char cur;
  TokenKind kind;
  Locatable location(file_name);

AdvanceToken:
  location.set_position(line, column);
  if (first() == eof_char) {
    Symbol symbol = symbols.internalize("");
    kind = TK_EOI;
    return Token(location, kind, symbol);
  }
  cur = first(), bump();
  switch (cur) {

  /// whitespace
  case ' ':  // space
  case '\t': // horizontal tab
  case '\v': // vertical tab
  case '\f': // form-feed
    eat_while(
        [](char c) { return c == ' ' || c == '\t' || c == '\v' || c == '\f'; });
    reset_pos_within_token();
    goto AdvanceToken;

  /// new line
  case '\r':
    switch (first()) {
    case '\n':
      bump();
      new_line();
      break;
    default:
      new_line();
    }
    goto AdvanceToken;
  case '\n':
    new_line();
    goto AdvanceToken;

  /// identifier
  case 'A' ... 'Z':
  case 'a' ... 'z':
  case '_':
    kind = identifier_or_keywords();
    break;

  /// constant

  /// decimal-constants, 0
  /// attention: this implementation regard "00" as valid constant
  case '0' ... '9':
    decimal_digits();
    kind = TK_CONSTANT;
    break;
  /// character-constant
  case '\'':
    if (not single_quoted_string(location))
      exit(1);
    kind = TK_CONSTANT;
    break;

  case '\"':
    if (not double_quoted_string(location))
      exit(1);
    kind = TK_STRING_LITERAL;
    break;

  /// punctuation
  case '!':
    switch (first()) {
    case '=':
      bump();
      kind = TK_EXCLAIMEQUAL; // "!="
      break;
    default:
      kind = TK_EXCLAIM; // "!"
    }
    break;
  case '#':
    switch (first()) {
    case '#':
      bump();
      kind = TK_HASHHASH; // "##"
      break;
    default:
      kind = TK_EXCLAIM; // "#"
    }
    break;
  case '%':
    switch (first()) {
    case ':':
      bump();
      if (first() == '%' && second() == ':') {
        bump(), bump();
        kind = TK_PERCENTCOLONPERCENTCOLON; // "%:%:"
      } else
        kind = TK_PERCENTCOLON; // "%:"
      break;
    case '=':
      bump();
      kind = TK_EXCLAIMEQUAL; // "%="
      break;
    case '>':
      bump();
      kind = TK_PERCENTGREATER; // "%="
      break;
    default:
      kind = TK_PERCENT; // "%"
    }
    break;
  case '&':
    switch (first()) {
    case '&':
      bump();
      kind = TK_AMPAMP; // "&&"
      break;
    case '=':
      bump();
      kind = TK_AMPEQUAL; // "&="
      break;
    default:
      kind = TK_AMP; //"&"
    }
    break;
  case '(':
    kind = TK_LPAREN;
    break;
  case ')':
    kind = TK_RPAREN;
    break;
  case '*':
    switch (first()) {
    case '=':
      bump();
      kind = TK_STAREQUAL; // "*="
      break;
    default:
      kind = TK_STAR; // "*"
    }
    break;
  case '+':
    switch (first()) {
    case '+':
      bump();
      kind = TK_PLUSPLUS; // "++"
      break;
    case '=':
      bump();
      kind = TK_PLUSEQUAL; // "+="
      break;
    default:
      kind = TK_PLUS; // "+"
    }
    break;
  case ',':
    kind = TK_COMMA;
    break;
  case '-':
    switch (first()) {
    case '-':
      bump();
      kind = TK_MINUSMINUS; // "--"
      break;
    case '=':
      bump();
      kind = TK_MINUSEQUAL; // "-="
      break;
    case '>':
      bump();
      kind = TK_ARROW; // "->"
      break;
    default:
      kind = TK_MINUS; // "-"
    }
    break;
  case '.':
    if (first() == '.' && second() == '.') {
      bump(), bump();
      kind = TK_ELLIPSIS; // "..."
    } else
      kind = TK_PERIOD;
    break;
  case '/':
    switch (first()) {
    case '=':
      bump();
      kind = TK_SLASHEQUAL; // "/="
      break;
    case '/':
      // line comment
      bump();
      line_comment();
      if (first() == eof_char) {
        // TODO: error msg
        errorloc(location, "\n");
        exit(1);
      }
      reset_pos_within_token();
      goto AdvanceToken;
      break;
    case '*':
      // block comment
      bump();
      if (not block_comment(location))
        exit(1);
      if (first() == eof_char) {
        // TODO: error msg
        errorloc(location, "\n");
        exit(1);
      }
      reset_pos_within_token();
      goto AdvanceToken;
    default:
      kind = TK_SLASH; // "/"
    }
    break;
  case ':':
    switch (first()) {
    case '>':
      bump();
      kind = TK_COLONGREATER; // ":>"
      break;
    default:
      kind = TK_COLON; // ":"
    }
    break;
  case '<':
    switch (first()) {
    case '%':
      bump();
      kind = TK_LESSPERCENT; // "<%"
      break;
    case ':':
      bump();
      kind = TK_LESSCOLON; // "<:"
      break;
    case '<':
      bump();
      switch (first()) {
      case '=':
        bump();
        kind = TK_LESSLESSEQUAL; // "<<="
        break;
      default:
        kind = TK_LESSLESS; // "<<"
      }
      break;
    case '=':
      bump();
      kind = TK_LESSEQUAL; // "<="
      break;
    default:
      kind = TK_LESS; // "<"
    }
    break;
  case '=':
    switch (first()) {
    case '=':
      bump();
      kind = TK_EQUALEQUAL; //"=="
      break;
    default:
      kind = TK_EQUAL; //"="
    }
    break;
  case '>':
    switch (first()) {
    case '=':
      bump();
      kind = TK_GREATEREQUAL; // ">="
      break;
    case '>':
      bump();
      switch (first()) {
      case '=':
        bump();
        kind = TK_GREATERGREATEREQUAL; // ">>="
        break;
      default:
        kind = TK_GREATERGREATER; // ">>"
      }
      break;
    default:
      kind = TK_GREATER; // ">"
    }
    break;
  case '?':
    kind = TK_QUESTION; // "?"
    break;
  case '[':
    kind = TK_LSQUARE; // "["
    break;
  case ']':
    kind = TK_RSQUARE; // "]"
    break;
  case '^':
    switch (first()) {
    case '=':
      bump();
      kind = TK_CARETEQUAL; // "^="
      break;
    default:
      kind = TK_CARET; // "^"
    }
    break;
  case '{':
    kind = TK_LBRACE; // "{"
    break;
  case '|':
    switch (first()) {
    case '=':
      bump();
      kind = TK_PIPEEQUAL; // "|="
      break;
    case '|':
      bump();
      kind = TK_PIPEPIPE; // "||"
      break;
    default:
      kind = TK_CARET; // "^"
    }
    break;
  case '}':
    kind = TK_RBRACE; // ""
    break;
  case ';':
    kind = TK_SEMICOLON; // ";"
    break;
  case '~':
    kind = TK_TILD; // "~"
    break;
  default:
    errorloc(location, " error: invaild charactor\n");
    exit(1);
  }

  std::string_view str = token_string(pos_within_token());
  Symbol symbol = symbols.internalize(str);
  Token tok(location, kind, symbol);
  reset_pos_within_token();
  return tok;
}

std::string_view Lexer::token_string(size_t len) {
  return std::string_view(chars - len, len);
}

TokenKind Lexer::identifier_or_keywords() {
  eat_while(Lexer::is_id_continue);
  std::string_view str = token_string(pos_within_token());
  return is_keyword(str) ? keyword_kind(str) : TK_IDENTIFIER;
}

void Lexer::decimal_digits() {
  eat_while([](char c) { return '0' <= c && c <= '9'; });
}

bool Lexer::single_quoted_string(const Locatable &loc) {
  if (first() == eof_char or first() == '\n' or first() == '\r') {
    errorloc(loc, "Missing terminating ' character\n");
    return false;
  }
  if (first() == '\'') {
    errorloc(loc, "Empty character constant\n");
    return false;
  }
  // escape sequence
  if (first() == '\\') {
    bump();
    if (first() == eof_char) {
      errorloc(loc, "Missing terminating ' character\n");
      return false;
    }
    if (not is_escape_sequence_char(first())) {
      errorloc(loc, "Unexpected escape sequence: \\", first(), '\n');
      return false;
    }
  }
  if (second() == eof_char) {
    errorloc(loc, "Missing terminating ' character\n");
    return false;
  }
  if (second() != '\'') {
    errorloc(loc, "Multi-character character constant\n");
    return false;
  }
  bump(), bump();
  return true;
}

bool Lexer::double_quoted_string(const Locatable &loc) {
  while (first() != eof_char) {
    switch (first()) {
    case '"':
      bump();
      return true;
    case '\\':
      if (second() != eof_char && not is_escape_sequence_char(second())) {
        errorloc(loc, "Unexpected escape sequence: \\", first(), '\n');
        return false;
      }
      bump();
      break;
    case '\n':
    case '\r':
      errorloc(loc, "Missing terminating '\"' character\n");
      return false;
    }
    bump();
  }
  errorloc(loc, "Unexpected escape sequence: \\", first(), '\n');
  return false;
}

void Lexer::bump() {
  column++;
  if (chars != end)
    chars++;
}

void Lexer::new_line() {
  column = 1;
  line++;
  reset_pos_within_token();
}

char Lexer::first() { return chars != end ? *chars : eof_char; }

char Lexer::second() { return (chars + 1) != end ? *(chars + 1) : eof_char; }

bool Lexer::is_eof() { return chars == end; }

size_t Lexer::pos_within_token() { return len_remaining - (end - chars); }

void Lexer::reset_pos_within_token() { len_remaining = end - chars; }

void Lexer::eat_while(bool (*predicate)(char)) {
  while (predicate(first()) && !is_eof())
    bump();
}

void Lexer::eat_while(bool (*predicate)(char, char)) {
  while (predicate(first(), second()) && !is_eof())
    bump();
}

bool Lexer::is_id_continue(char c) { return isalnum(c) || c == '_'; }

bool Lexer::is_keyword(std::string_view s) {
  return keyword_map.find(s) != keyword_map.end();
}

bool Lexer::is_escape_sequence_char(char c) {
  return c == '\'' || c == '\"' || c == '?' || c == '\\' || c == 'a' ||
         c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v';
}

void Lexer::line_comment() {
  eat_while([](char c) { return c != '\r' && c != '\n'; });
}

bool Lexer::block_comment(const Locatable &loc) {
  char cur;
EatBlockComment:
  eat_while([](char first, char second) {
    return // */
        !(first == '*' && second == '/') &&
        // new line
        (first != '\r' && first != '\n');
  });
  if (first() == eof_char) {
    errorloc(loc, "Unterminated /* comment\n");
    return false;
  }
  cur = first(), bump();
  switch (cur) {
  case '*':
    if (first() == '/') {
      bump();
      return true;
    }
    break;
  case '\r':
    switch (first()) {
    case '\n':
      bump();
      new_line();
      break;
    default:
      new_line();
    }
    goto EatBlockComment;
  case '\n':
    new_line();
    goto EatBlockComment;
  default:
    goto EatBlockComment;
  }
  return false;
}

TokenKind Lexer::keyword_kind(std::string_view s) { return keyword_map[s]; }
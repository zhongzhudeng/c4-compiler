#include "Lexer.hpp"

#include <optional>
#include <string_view>

#include "Token.hpp"

Lexer::Lexer(std::string_view file_name, std::string_view input) {
  line = 1;
  column = 1;
  len_remaining = input.length();
  chars = input.begin();
  end = input.cend();
  this->file_name = file_name;
}

Lexer::~Lexer() {}

void Lexer::tokenize() {
  for (;;) {
    auto tok = advance_token();
    if (tok.Kind == TokenKind::TK_EOI){
      break;
    } 
    if (first()==eof_char) {
      Locatable location(file_name, line, column);
      std::cerr << location << " error: Unexpected EOF\n";
      break;
    }
    std::cout << tok << "\n";
  }
}

Token Lexer::advance_token() {
  TokenKind kind;
  std::optional<char> first_char_opt;
  char first_char;
  bool success = true;

AdvanceToken:
  Locatable location(file_name, line, column);
  first_char_opt = bump();
  if (not first_char_opt.has_value()) {
    Symbol symbol = symbols.internalize("");
    kind = TK_EOI;
    return Token(location, kind, symbol);
  }

  first_char = first_char_opt.value();
  switch (first_char) {

  /// whitespace
  case '\t':
  case '':
  case '':
  case ' ':
    eat_while([](char c) { return c == '\t' || c == ' '; });
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
    success = single_quoted_string();
    kind = TK_CONSTANT;
    break;

  case '\"':
    success = double_quoted_string();
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
      eat_line_comment();
      reset_pos_within_token();
      if (first()==eof_char) {
        std::cerr << location << " error: Unexpected EOF\n";
        exit(1);
      }
      goto AdvanceToken;
      break;
    case '*':
      // block comment
      bump();
      success = eat_block_comment();
      if (not success)
        goto NotSuccess;
      reset_pos_within_token();
      if (first()==eof_char) {
        std::cerr << location << " error: Unexpected EOF\n";
        exit(1);
      }
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
    // default:
  }
  // TODO: implementation
  if (not success) {
  NotSuccess:
    std::cerr << location << " error\n";
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

bool Lexer::single_quoted_string() {
  if (first() == '\n' || first() == '\r') {
    return false;
  }
  // one symbol
  if (second() == '\'' && first() != '\\' && first() != '\'') {
    bump(), bump();
    return true;
  }
  // two symbols
  if (first() == '\\') {
    bump();
    if (second() == '\'' && is_escape_sequnce_char(first())) {
      bump(), bump();
      return true;
    }
  }
  return false;
}

bool Lexer::double_quoted_string() {
  auto first_char_opt = bump();
  while (first_char_opt.has_value()) {
    char first_char = first_char_opt.value();
    switch (first_char) {
    case '"':
      return true;
    case '\\':
      if (is_escape_sequnce_char(first()))
        bump();
      else
        return false;
      break;
    case '\n':
    case '\r':
      return false;
    }
    first_char_opt = bump();
  }
  return false;
}

std::optional<char> Lexer::bump() {
  column++;
  return chars != end ? std::optional<char>{*chars++} : std::nullopt;
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
  while (predicate(first(), second()) && chars + 1 != end)
    bump();
}

bool Lexer::is_id_continue(char c) { return isalnum(c) || c == '_'; }

bool Lexer::is_keyword(std::string_view s) {
  return keyword_map.find(s) != keyword_map.end();
}

bool Lexer::is_escape_sequnce_char(char c) {
  return c == '\'' || c == '\"' || c == '?' || c == '\\' || c == 'a' ||
         c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v';
}

void Lexer::eat_line_comment() {
  eat_while([](char c) { return c != '\r' && c != '\n'; });
}

bool Lexer::eat_block_comment() {
  std::optional<char> first_char_opt;
  char first_char;
EatBlockComment:
  eat_while([](char first, char second) {
    return // */
        !(first == '*' && second == '/') &&
        // new line
        (first != '\r' && first != '\n');
  });

  first_char_opt = bump();
  if (not first_char_opt.has_value())
    return false;
  first_char = first_char_opt.value();

  switch (first_char) {
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
  }
  if (first_char == '*' && first() == '/') {
    bump();
    return true;
  }
  return false;
}

TokenKind Lexer::keyword_kind(std::string_view s) { return keyword_map[s]; }
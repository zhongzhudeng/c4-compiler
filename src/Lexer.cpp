#include "Lexer.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>

namespace {

struct Punctuator {
  const char *text;
  TokenKind kind;
};

// Longest spellings must be tried first.  This is the complete C11
// punctuator set; unsupported operators are still lexed and are rejected by
// the parser later.
constexpr Punctuator kPunctuators[] = {
    {"%:%:", TK_PERCENTCOLONPERCENTCOLON},
    {"<<=", TK_LESSLESSEQUAL},
    {">>=", TK_GREATERGREATEREQUAL},
    {"...", TK_ELLIPSIS},
    {"##", TK_HASHHASH},
    {"->", TK_ARROW},
    {"++", TK_PLUSPLUS},
    {"--", TK_MINUSMINUS},
    {"<<", TK_LESSLESS},
    {">>", TK_GREATERGREATER},
    {"<=", TK_LESSEQUAL},
    {">=", TK_GREATEREQUAL},
    {"==", TK_EQUALEQUAL},
    {"!=", TK_EXCLAIMEQUAL},
    {"&&", TK_AMPAMP},
    {"||", TK_PIPEPIPE},
    {"*=", TK_STAREQUAL},
    {"/=", TK_SLASHEQUAL},
    {"%=", TK_PERCENTEQUAL},
    {"+=", TK_PLUSEQUAL},
    {"-=", TK_MINUSEQUAL},
    {"&=", TK_AMPEQUAL},
    {"^=", TK_CARETEQUAL},
    {"|=", TK_PIPEEQUAL},
    {"<:", TK_LESSCOLON},
    {":>", TK_COLONGREATER},
    {"<%", TK_LESSPERCENT},
    {"%>", TK_PERCENTGREATER},
    {"%:", TK_PERCENTCOLON},
    {"[", TK_LSQUARE},
    {"]", TK_RSQUARE},
    {"(", TK_LPAREN},
    {")", TK_RPAREN},
    {"{", TK_LBRACE},
    {"}", TK_RBRACE},
    {".", TK_PERIOD},
    {"&", TK_AMP},
    {"*", TK_STAR},
    {"+", TK_PLUS},
    {"-", TK_MINUS},
    {"~", TK_TILD},
    {"!", TK_EXCLAIM},
    {"/", TK_SLASH},
    {"%", TK_PERCENT},
    {"<", TK_LESS},
    {">", TK_GREATER},
    {"^", TK_CARET},
    {"|", TK_PIPE},
    {"?", TK_QUESTION},
    {":", TK_COLON},
    {";", TK_SEMICOLON},
    {"=", TK_EQUAL},
    {",", TK_COMMA},
    {"#", TK_HASH},
};

} // namespace

Lexer::Lexer(std::string filename, SymbolInternalizer &symbols)
    : filename_(std::move(filename)), symbols_(symbols),
      diagnostics_(filename_) {
  filenameSymbol_ = symbols_.internalize(filename_);
  std::ifstream stream(filename_, std::ios::binary);
  if (!stream) {
    diagnostics_.error(SourceLoc{1, 1}, "cannot open input file");
  }
  source_.assign(std::istreambuf_iterator<char>(stream),
                 std::istreambuf_iterator<char>());
}

Lexer::Lexer(std::string filename, std::string source,
             SymbolInternalizer &symbols)
    : filename_(std::move(filename)), source_(std::move(source)),
      symbols_(symbols), diagnostics_(filename_) {
  filenameSymbol_ = symbols_.internalize(filename_);
}

int Lexer::peek(size_t offset) const {
  if (pos_ + offset >= source_.size())
    return -1;
  return static_cast<unsigned char>(source_[pos_ + offset]);
}

bool Lexer::eof(size_t offset) const { return pos_ + offset >= source_.size(); }

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

bool Lexer::isIdentStart(int c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool Lexer::isIdentContinue(int c) { return isIdentStart(c) || isDigit(c); }

bool Lexer::isDigit(int c) { return c >= '0' && c <= '9'; }

bool Lexer::isSimpleEscape(int c) {
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

void Lexer::skipWhitespaceAndComments() {
  for (;;) {
    int c = peek();
    if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' ||
        c == '\n') {
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

void Lexer::fail(SourceLoc loc, const std::string &message) const {
  diagnostics_.error(loc, message);
}

Token Lexer::makeToken(TokenKind kind, SourceLoc start, size_t begin) const {
  std::string_view spelling(source_.data() + begin, pos_ - begin);
  return Token(start, kind, symbols_.internalize(spelling), filenameSymbol_);
}

TokenKind Lexer::lexNumber() {
  if (peek() == '0') {
    consume();
    if (isDigit(peek()))
      fail(here(), "octal constants are not supported");
    if (isIdentStart(peek()) || peek() == '.')
      fail(here(), "invalid suffix on integer constant");
    return TK_CONSTANT;
  }
  while (isDigit(peek()))
    consume();
  if (isIdentStart(peek()) || peek() == '.')
    fail(here(), "invalid suffix on integer constant");
  return TK_CONSTANT;
}

TokenKind Lexer::lexCharacterConstant() {
  SourceLoc start = here();
  consume(); // opening quote
  if (eof() || peek() == '\n' || peek() == '\r')
    fail(start, "missing terminating ' character");
  if (peek() == '\'')
    fail(here(), "empty character constant");
  if (peek() == '\\') {
    consume();
    if (eof() || peek() == '\n' || peek() == '\r')
      fail(start, "missing terminating ' character");
    if (!isSimpleEscape(peek()))
      fail(here(), "invalid escape sequence");
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
  return TK_CONSTANT;
}

TokenKind Lexer::lexStringLiteral() {
  SourceLoc start = here();
  consume(); // opening quote
  while (!eof()) {
    int c = peek();
    if (c == '"') {
      consume();
      return TK_STRING_LITERAL;
    }
    if (c == '\n' || c == '\r')
      fail(start, "missing terminating \" character");
    if (c == '\\') {
      consume();
      if (eof() || peek() == '\n' || peek() == '\r')
        fail(start, "missing terminating \" character");
      if (!isSimpleEscape(peek()))
        fail(here(), "invalid escape sequence");
      consume();
      continue;
    }
    if (c == 0)
      fail(here(), "invalid character in string literal");
    consume();
  }
  fail(start, "missing terminating \" character");
}

TokenKind Lexer::lexPunctuator() {
  for (const Punctuator &p : kPunctuators) {
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

Token Lexer::lexToken() {
  SourceLoc start = here();
  size_t begin = pos_;
  int c = peek();
  TokenKind kind;
  if (isIdentStart(c)) {
    while (isIdentContinue(peek()))
      consume();
    std::string_view text(source_.data() + begin, pos_ - begin);
#define KEYWORD_ACTION(KD, STR)                                                \
  if (text == STR)                                                             \
    return makeToken(KD, start, begin);
#include "KeyWords.def"
#undef KEYWORD_ACTION
    kind = TK_IDENTIFIER;
  } else if (isDigit(c)) {
    kind = lexNumber();
  } else if (c == '\'') {
    kind = lexCharacterConstant();
  } else if (c == '"') {
    kind = lexStringLiteral();
  } else {
    kind = lexPunctuator();
  }
  return makeToken(kind, start, begin);
}

void Lexer::tokenize(const std::function<void(const Token &)> &f) {
  for (const Token &tok : tokenize()) {
    if (tok.Kind != TK_EOI)
      f(tok);
  }
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  for (;;) {
    skipWhitespaceAndComments();
    SourceLoc start = here();
    if (eof()) {
      tokens.push_back(
          Token(start, TK_EOI, symbols_.internalize(""), filenameSymbol_));
      return tokens;
    }
    tokens.push_back(lexToken());
  }
}

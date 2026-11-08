#pragma once

#include "Support.hpp"
#include "SymbolInternalizer.hpp"
#include "Token.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

/// Hand-written lexer for the C4 subset.  It implements the maximal-munch
/// rule, ISO-8859-1 byte positions, CR/LF/CRLF line endings, and the
/// restricted constant/string rules from the assignment.
class Lexer {
public:
  Lexer(std::string filename, SymbolInternalizer &symbols);
  Lexer(std::string filename, std::string source,
        SymbolInternalizer &symbols);

  Lexer(const Lexer &) = delete;
  Lexer &operator=(const Lexer &) = delete;

  /// Calls f for every token except the internal end-of-input token.
  void tokenize(const std::function<void(const Token &)> &f);

  /// Returns the complete token stream, including the end-of-input token.
  std::vector<Token> tokenize();

private:
  std::string filename_;
  std::string source_;
  size_t pos_ = 0;
  uint32_t line_ = 1;
  uint32_t column_ = 1;
  SymbolInternalizer &symbols_;
  Diagnostics diagnostics_;
  Symbol filenameSymbol_ = nullptr;

  int peek(size_t offset = 0) const;
  bool eof(size_t offset = 0) const;
  SourceLoc here() const { return SourceLoc{line_, column_}; }
  void consume();

  void skipWhitespaceAndComments();
  Token lexToken();
  TokenKind lexNumber();
  TokenKind lexCharacterConstant();
  TokenKind lexStringLiteral();
  TokenKind lexPunctuator();

  Token makeToken(TokenKind kind, SourceLoc start, size_t begin) const;
  [[noreturn]] void fail(SourceLoc loc, const std::string &message) const;

  static bool isIdentStart(int c);
  static bool isIdentContinue(int c);
  static bool isDigit(int c);
  static bool isSimpleEscape(int c);
};

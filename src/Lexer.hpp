#pragma once

#include "SymbolInternalizer.hpp"
#include "Token.hpp"
#include <functional>
#include <string>
#include <string_view>

class Lexer {
public:
  Lexer(const char *filename, SymbolInternalizer &symbolInternalizer);
  /// For unittest usage
  Lexer(const char *filename, std::string src,
        SymbolInternalizer &symbolInternalizer);
  ~Lexer();

  void tokenize(std::function<void(Token)> f);
  std::vector<Token> tokenize();

private:
  std::string_view filename;
  std::string src;
  std::string_view::iterator chars;
  std::string_view::const_iterator end;
  size_t line;
  size_t column;
  size_t len_remaining;
  SymbolInternalizer &symbolInternalizer;

  static constexpr char eof_char = '\0';

  Token advance_token();
  std::string_view token_string(size_t len);
  TokenKind identifier_or_keywords();
  void decimal_digits();
  bool single_quoted_string(const Locatable &loc);
  bool double_quoted_string(const Locatable &loc);

  /// Moves to the next character.
  void bump();
  /// Moves to the next line.
  void new_line();
  /// Peeks the next symbol from the input stream without consuming it.
  /// If requested position doesn't exist, `EOF_CHAR` is returned.
  /// However, getting `EOF_CHAR` doesn't always mean actual end of file,
  /// it should be checked with `is_eof` method.
  char first();
  /// Peeks the second symbol from the input stream without consuming it.
  char second();
  /// Checks if there is nothing more to consume.
  bool is_eof();
  /// Returns amount of already consumed symbols.
  size_t pos_within_token();
  /// Resets the number of bytes consumed to 0.
  void reset_pos_within_token();
  /// Eats symbols while predicate returns true or until the end of file is
  /// reached.
  void eat_while(bool (*predicate)(char));
  void eat_while(bool (*predicate)(char, char));
  bool is_escape_sequence_char(char c);
  void line_comment();
  bool block_comment(const Locatable &loc);
};
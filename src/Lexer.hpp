#include <optional>
#include <queue>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "Token.hpp"

class Lexer {
public:
  Lexer(const char *file_name);
  ~Lexer();

  void tokenize();
  std::queue<Token *> lex();
  Token advance_token();

private:
  size_t line;
  size_t column;
  size_t len_remaining;

  static const char eof_char = '\0';

  std::string_view file_name;
  std::string input;
  std::string_view::iterator chars;
  std::string_view::const_iterator end;
  SymbolInternalizer symbols;
  /// A map contains <keywords_litteral, token_kind>
  std::unordered_map<std::string_view, TokenKind> keyword_map{
#define KEYWORD_ACTION(KD, STR) {STR, KD},
#include "KeyWords.def"
#undef KEYWORD_ACTION
  };

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
  /// True if `c` is valid as a non-first character of an identifier.
  static bool is_id_continue(char c);
  bool is_keyword(std::string_view s);
  bool is_escape_sequence_char(char c);
  void line_comment();
  bool block_comment(const Locatable &loc);
  TokenKind keyword_kind(std::string_view s);
};
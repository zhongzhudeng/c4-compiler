#include "Lexer.hpp"
#include "SymbolInternalizer.hpp"
#include "Support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

struct LexerFixture {
  SymbolInternalizer symbols;

  std::vector<Token> lex(const std::string &source) {
    Lexer lexer("test.c", source, symbols);
    return lexer.tokenize();
  }
};

} // namespace

TEST_CASE("lexer reports exact token positions and spellings", "[lexer]") {
  LexerFixture fixture;
  std::vector<Token> tokens = fixture.lex("42 if\n  \"bla\\n\"x+");
  REQUIRE(tokens.size() == 6);

  REQUIRE(tokens[0].Kind == TK_CONSTANT);
  REQUIRE(*tokens[0].Text == "42");
  REQUIRE(tokens[0].Loc == (SourceLoc{1, 1}));

  REQUIRE(tokens[1].Kind == TK_IF);
  REQUIRE(*tokens[1].Text == "if");
  REQUIRE(tokens[1].Loc == (SourceLoc{1, 4}));

  REQUIRE(tokens[2].Kind == TK_STRING_LITERAL);
  REQUIRE(*tokens[2].Text == "\"bla\\n\"");
  REQUIRE(tokens[2].Loc == (SourceLoc{2, 3}));

  REQUIRE(tokens[3].Kind == TK_IDENTIFIER);
  REQUIRE(*tokens[3].Text == "x");
  REQUIRE(tokens[3].Loc == (SourceLoc{2, 10}));

  REQUIRE(tokens[4].Kind == TK_PLUS);
  REQUIRE(*tokens[4].Text == "+");
  REQUIRE(tokens[4].Loc == (SourceLoc{2, 11}));

  REQUIRE(tokens[5].Kind == TK_EOI);
  REQUIRE(tokens[5].Loc == (SourceLoc{2, 12}));
}

TEST_CASE("lexer accepts comments at end of input", "[lexer]") {
  LexerFixture fixture;
  REQUIRE(fixture.lex("int x; // comment").size() == 4);
  REQUIRE(fixture.lex("int x; /* comment */").size() == 4);
  REQUIRE_THROWS_AS(fixture.lex("/* unterminated"), CompileError);
}

TEST_CASE("lexer rejects constants outside the C4 subset", "[lexer]") {
  LexerFixture fixture;
  REQUIRE_THROWS_AS(fixture.lex("00"), CompileError);
  REQUIRE_THROWS_AS(fixture.lex("012"), CompileError);
  REQUIRE_THROWS_AS(fixture.lex("42u"), CompileError);
  REQUIRE_THROWS_AS(fixture.lex("42abc"), CompileError);
  REQUIRE_THROWS_AS(fixture.lex("0x10"), CompileError);
  REQUIRE_THROWS_AS(fixture.lex("1.0"), CompileError);
}

TEST_CASE("lexer recognizes digraph token kinds", "[lexer]") {
  LexerFixture fixture;
  std::vector<Token> tokens = fixture.lex("<: :> <% %> %: %:%:");
  REQUIRE(tokens.size() == 7);
  REQUIRE(tokens[0].Kind == TK_LESSCOLON);
  REQUIRE(tokens[1].Kind == TK_COLONGREATER);
  REQUIRE(tokens[2].Kind == TK_LESSPERCENT);
  REQUIRE(tokens[3].Kind == TK_PERCENTGREATER);
  REQUIRE(tokens[4].Kind == TK_PERCENTCOLON);
  REQUIRE(tokens[5].Kind == TK_PERCENTCOLONPERCENTCOLON);
  REQUIRE(canonicalTokenKind(tokens[0].Kind) == TK_LSQUARE);
  REQUIRE(canonicalTokenKind(tokens[1].Kind) == TK_RSQUARE);
  REQUIRE(canonicalTokenKind(tokens[2].Kind) == TK_LBRACE);
  REQUIRE(canonicalTokenKind(tokens[3].Kind) == TK_RBRACE);
  REQUIRE(canonicalTokenKind(tokens[4].Kind) == TK_HASH);
}

TEST_CASE("lexer handles CRLF and lone CR line endings", "[lexer]") {
  LexerFixture fixture;
  std::vector<Token> tokens = fixture.lex("a\r\nb\rc");
  REQUIRE(tokens.size() == 4);
  REQUIRE(tokens[0].Loc == (SourceLoc{1, 1}));
  REQUIRE(tokens[1].Loc == (SourceLoc{2, 1}));
  REQUIRE(tokens[2].Loc == (SourceLoc{3, 1}));
}

TEST_CASE("lexer preserves string and character spellings", "[lexer]") {
  LexerFixture fixture;
  std::vector<Token> tokens = fixture.lex("\"a\\tb\" '\\n'");
  REQUIRE(tokens.size() == 3);
  REQUIRE(*tokens[0].Text == "\"a\\tb\"");
  REQUIRE(*tokens[1].Text == "'\\n'");
}

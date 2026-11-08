#include "Compiler.hpp"
#include "Lexer.hpp"
#include "SymbolInternalizer.hpp"
#include "Support.hpp"
#include "TestSupport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <fstream>
#include <iterator>
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

std::string diagnosticFor(LexerFixture &fixture, const std::string &source) {
  ScopedCerrCapture capture;
  try {
    fixture.lex(source);
  } catch (const CompileError &) {
    return capture.text();
  }
  return {};
}

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

TEST_CASE("lexer applies maximal munch", "[lexer]") {
  LexerFixture fixture;
  std::vector<Token> tokens = fixture.lex("a+++b <<= 2 ...");
  REQUIRE(tokens.size() == 8);
  REQUIRE(tokens[0].Kind == TK_IDENTIFIER);
  REQUIRE(tokens[1].Kind == TK_PLUSPLUS);
  REQUIRE(*tokens[1].Text == "++");
  REQUIRE(tokens[2].Kind == TK_PLUS);
  REQUIRE(tokens[3].Kind == TK_IDENTIFIER);
  REQUIRE(tokens[4].Kind == TK_LESSLESSEQUAL);
  REQUIRE(*tokens[4].Text == "<<=");
  REQUIRE(tokens[5].Kind == TK_CONSTANT);
  REQUIRE(tokens[6].Kind == TK_ELLIPSIS);
  REQUIRE(*tokens[6].Text == "...");
  REQUIRE(tokens[7].Kind == TK_EOI);

  tokens = fixture.lex("x-->y");
  REQUIRE(tokens.size() == 5);
  REQUIRE(tokens[1].Kind == TK_MINUSMINUS);
  REQUIRE(*tokens[1].Text == "--");
  REQUIRE(tokens[2].Kind == TK_GREATER);
  REQUIRE(tokens[3].Kind == TK_IDENTIFIER);
}

TEST_CASE("lexer tracks tabs and mixed line endings", "[lexer]") {
  LexerFixture fixture;
  std::vector<Token> tokens = fixture.lex("a\tb\r\n\tc\rd");
  REQUIRE(tokens.size() == 5);
  REQUIRE(tokens[0].Loc == (SourceLoc{1, 1}));
  REQUIRE(tokens[1].Loc == (SourceLoc{1, 3}));
  REQUIRE(tokens[2].Loc == (SourceLoc{2, 2}));
  REQUIRE(tokens[3].Loc == (SourceLoc{3, 1}));
}

TEST_CASE("lexer handles empty input and trailing comments", "[lexer]") {
  LexerFixture fixture;
  std::vector<Token> tokens = fixture.lex("");
  REQUIRE(tokens.size() == 1);
  REQUIRE(tokens[0].Kind == TK_EOI);
  REQUIRE(tokens[0].Loc == (SourceLoc{1, 1}));
  REQUIRE(tokens[0].Text);
  REQUIRE(tokens[0].Text->empty());

  tokens = fixture.lex("x // comment");
  REQUIRE(tokens.size() == 2);
  REQUIRE(tokens[1].Loc == (SourceLoc{1, 13}));

  tokens = fixture.lex("x /* c */");
  REQUIRE(tokens.size() == 2);
  REQUIRE(tokens[1].Loc == (SourceLoc{1, 10}));

  REQUIRE_THROWS_AS(fixture.lex("\"abc"), CompileError);
  REQUIRE_THROWS_AS(fixture.lex("'a"), CompileError);
}

TEST_CASE("lexer reports exact diagnostic locations and messages", "[lexer]") {
  LexerFixture fixture;
  REQUIRE(diagnosticFor(fixture, "42u") ==
          "test.c:1:3: error: invalid suffix on integer constant\n");
  REQUIRE(diagnosticFor(fixture, "00") ==
          "test.c:1:2: error: octal constants are not supported\n");
  REQUIRE(diagnosticFor(fixture, "'\\q'") ==
          "test.c:1:2: error: invalid escape sequence\n");
  REQUIRE(diagnosticFor(fixture, "\"a\\q\"") ==
          "test.c:1:3: error: invalid escape sequence\n");
  REQUIRE(diagnosticFor(fixture, "1.0") ==
          "test.c:1:2: error: invalid suffix on integer constant\n");
  REQUIRE(diagnosticFor(fixture, "'ab'") ==
          "test.c:1:3: error: multi-character character constant\n");
  REQUIRE(diagnosticFor(fixture, "/*") ==
          "test.c:1:1: error: unterminated comment\n");
  REQUIRE(diagnosticFor(fixture, "@") ==
          "test.c:1:1: error: invalid character '@'\n");
  REQUIRE(diagnosticFor(fixture, "caf\xE9") ==
          "test.c:1:4: error: invalid character (byte 233)\n");
}

TEST_CASE("lexer processes large inputs in one pass", "[lexer]") {
  LexerFixture fixture;
  const int lines = 100000;
  std::string source;
  source.reserve(static_cast<size_t>(lines) * 12);
  for (int i = 0; i < lines; ++i) {
    source += "int x";
    source += std::to_string(i);
    source += ";\n";
  }

  std::vector<Token> tokens = fixture.lex(source);
  REQUIRE(tokens.size() == static_cast<size_t>(3 * lines + 1));
  REQUIRE(tokens[tokens.size() - 2].Kind == TK_SEMICOLON);
  REQUIRE(tokens[tokens.size() - 2].Loc.line ==
          static_cast<uint32_t>(lines));
  const Token &lastIdentifier = tokens[tokens.size() - 3];
  REQUIRE(lastIdentifier.Kind == TK_IDENTIFIER);
  REQUIRE(*lastIdentifier.Text == "x" + std::to_string(lines - 1));
  REQUIRE(tokens.back().Kind == TK_EOI);
  REQUIRE(tokens.back().Loc ==
          (SourceLoc{static_cast<uint32_t>(lines + 1), 1}));
}

TEST_CASE("file and string tokenization agree", "[lexer]") {
  const std::string path = std::string(C4_FIXTURE_DIR) + "/pretty.c";
  std::ifstream stream(path, std::ios::binary);
  REQUIRE(stream.good());
  std::string source;
  source.assign(std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());

  TokenizedSource fromString = tokenizeString(path, source);
  TokenizedSource fromFile = tokenizeFile(path);
  REQUIRE(fromString.tokens().size() == fromFile.tokens().size());
  for (size_t i = 0; i < fromString.tokens().size(); ++i) {
    const Token &stringToken = fromString.tokens()[i];
    const Token &fileToken = fromFile.tokens()[i];
    CHECK(stringToken.Kind == fileToken.Kind);
    CHECK(stringToken.Loc == fileToken.Loc);
    CHECK(*stringToken.Text == *fileToken.Text);
    CHECK(*stringToken.FileName == *fileToken.FileName);
  }
}

TEST_CASE("tokenizing a missing file reports the source diagnostic",
          "[lexer]") {
  ScopedCerrCapture capture;
  REQUIRE_THROWS_AS(tokenizeFile("/nonexistent/c4-missing-input.c"),
                    CompileError);
  REQUIRE(capture.text() ==
          "/nonexistent/c4-missing-input.c:1:1: error: cannot open input file\n");
}

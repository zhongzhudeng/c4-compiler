#include "Compiler.hpp"
#include "Lexer.hpp"
#include "Support.hpp"
#include "SymbolInternalizer.hpp"
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

    std::vector<Token> lex(const std::string& source) {
        Lexer lexer("test.c", source, symbols);
        return lexer.tokenize();
    }
};

std::string diagnostic_for(LexerFixture& fixture, const std::string& source) {
    ScopedCerrCapture capture;
    try {
        fixture.lex(source);
    } catch (const CompileError&) {
        return capture.text();
    }
    return {};
}

} // namespace

TEST_CASE("lexer reports exact token positions and spellings", "[lexer]") {
    LexerFixture fixture;
    std::vector<Token> tokens = fixture.lex("42 if\n  \"bla\\n\"x+");
    REQUIRE(tokens.size() == 6);

    REQUIRE(tokens[0].kind == TkConstant);
    REQUIRE(*tokens[0].text == "42");
    REQUIRE(tokens[0].loc == (SourceLoc{1, 1}));

    REQUIRE(tokens[1].kind == TkIf);
    REQUIRE(*tokens[1].text == "if");
    REQUIRE(tokens[1].loc == (SourceLoc{1, 4}));

    REQUIRE(tokens[2].kind == TkStringLiteral);
    REQUIRE(*tokens[2].text == "\"bla\\n\"");
    REQUIRE(tokens[2].loc == (SourceLoc{2, 3}));

    REQUIRE(tokens[3].kind == TkIdentifier);
    REQUIRE(*tokens[3].text == "x");
    REQUIRE(tokens[3].loc == (SourceLoc{2, 10}));

    REQUIRE(tokens[4].kind == TkPlus);
    REQUIRE(*tokens[4].text == "+");
    REQUIRE(tokens[4].loc == (SourceLoc{2, 11}));

    REQUIRE(tokens[5].kind == TkEoi);
    REQUIRE(tokens[5].loc == (SourceLoc{2, 12}));
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
    REQUIRE(tokens[0].kind == TkLesscolon);
    REQUIRE(tokens[1].kind == TkColongreater);
    REQUIRE(tokens[2].kind == TkLesspercent);
    REQUIRE(tokens[3].kind == TkPercentgreater);
    REQUIRE(tokens[4].kind == TkPercentcolon);
    REQUIRE(tokens[5].kind == TkPercentcolonpercentcolon);
    REQUIRE(canonical_token_kind(tokens[0].kind) == TkLsquare);
    REQUIRE(canonical_token_kind(tokens[1].kind) == TkRsquare);
    REQUIRE(canonical_token_kind(tokens[2].kind) == TkLbrace);
    REQUIRE(canonical_token_kind(tokens[3].kind) == TkRbrace);
    REQUIRE(canonical_token_kind(tokens[4].kind) == TkHash);
}

TEST_CASE("lexer handles CRLF and lone CR line endings", "[lexer]") {
    LexerFixture fixture;
    std::vector<Token> tokens = fixture.lex("a\r\nb\rc");
    REQUIRE(tokens.size() == 4);
    REQUIRE(tokens[0].loc == (SourceLoc{1, 1}));
    REQUIRE(tokens[1].loc == (SourceLoc{2, 1}));
    REQUIRE(tokens[2].loc == (SourceLoc{3, 1}));
}

TEST_CASE("lexer preserves string and character spellings", "[lexer]") {
    LexerFixture fixture;
    std::vector<Token> tokens = fixture.lex("\"a\\tb\" '\\n'");
    REQUIRE(tokens.size() == 3);
    REQUIRE(*tokens[0].text == "\"a\\tb\"");
    REQUIRE(*tokens[1].text == "'\\n'");
}

TEST_CASE("lexer applies maximal munch", "[lexer]") {
    LexerFixture fixture;
    std::vector<Token> tokens = fixture.lex("a+++b <<= 2 ...");
    REQUIRE(tokens.size() == 8);
    REQUIRE(tokens[0].kind == TkIdentifier);
    REQUIRE(tokens[1].kind == TkPlusplus);
    REQUIRE(*tokens[1].text == "++");
    REQUIRE(tokens[2].kind == TkPlus);
    REQUIRE(tokens[3].kind == TkIdentifier);
    REQUIRE(tokens[4].kind == TkLesslessequal);
    REQUIRE(*tokens[4].text == "<<=");
    REQUIRE(tokens[5].kind == TkConstant);
    REQUIRE(tokens[6].kind == TkEllipsis);
    REQUIRE(*tokens[6].text == "...");
    REQUIRE(tokens[7].kind == TkEoi);

    tokens = fixture.lex("x-->y");
    REQUIRE(tokens.size() == 5);
    REQUIRE(tokens[1].kind == TkMinusminus);
    REQUIRE(*tokens[1].text == "--");
    REQUIRE(tokens[2].kind == TkGreater);
    REQUIRE(tokens[3].kind == TkIdentifier);
}

TEST_CASE("lexer tracks tabs and mixed line endings", "[lexer]") {
    LexerFixture fixture;
    std::vector<Token> tokens = fixture.lex("a\tb\r\n\tc\rd");
    REQUIRE(tokens.size() == 5);
    REQUIRE(tokens[0].loc == (SourceLoc{1, 1}));
    REQUIRE(tokens[1].loc == (SourceLoc{1, 3}));
    REQUIRE(tokens[2].loc == (SourceLoc{2, 2}));
    REQUIRE(tokens[3].loc == (SourceLoc{3, 1}));
}

TEST_CASE("lexer handles empty input and trailing comments", "[lexer]") {
    LexerFixture fixture;
    std::vector<Token> tokens = fixture.lex("");
    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].kind == TkEoi);
    REQUIRE(tokens[0].loc == (SourceLoc{1, 1}));
    REQUIRE(tokens[0].text);
    REQUIRE(tokens[0].text->empty());

    tokens = fixture.lex("x // comment");
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[1].loc == (SourceLoc{1, 13}));

    tokens = fixture.lex("x /* c */");
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[1].loc == (SourceLoc{1, 10}));

    REQUIRE_THROWS_AS(fixture.lex("\"abc"), CompileError);
    REQUIRE_THROWS_AS(fixture.lex("'a"), CompileError);
}

TEST_CASE("lexer reports exact diagnostic locations and messages", "[lexer]") {
    LexerFixture fixture;
    REQUIRE(diagnostic_for(fixture, "42u") ==
            "test.c:1:3: error: invalid suffix on integer constant\n");
    REQUIRE(diagnostic_for(fixture, "00") ==
            "test.c:1:2: error: octal constants are not supported\n");
    REQUIRE(diagnostic_for(fixture, "'\\q'") == "test.c:1:2: error: invalid escape sequence\n");
    REQUIRE(diagnostic_for(fixture, "\"a\\q\"") == "test.c:1:3: error: invalid escape sequence\n");
    REQUIRE(diagnostic_for(fixture, "1.0") ==
            "test.c:1:2: error: invalid suffix on integer constant\n");
    REQUIRE(diagnostic_for(fixture, "'ab'") ==
            "test.c:1:3: error: multi-character character constant\n");
    REQUIRE(diagnostic_for(fixture, "/*") == "test.c:1:1: error: unterminated comment\n");
    REQUIRE(diagnostic_for(fixture, "@") == "test.c:1:1: error: invalid character '@'\n");
    REQUIRE(diagnostic_for(fixture, "caf\xE9") ==
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
    REQUIRE(tokens[tokens.size() - 2].kind == TkSemicolon);
    REQUIRE(tokens[tokens.size() - 2].loc.line == static_cast<uint32_t>(lines));
    const Token& last_identifier = tokens[tokens.size() - 3];
    REQUIRE(last_identifier.kind == TkIdentifier);
    REQUIRE(*last_identifier.text == "x" + std::to_string(lines - 1));
    REQUIRE(tokens.back().kind == TkEoi);
    REQUIRE(tokens.back().loc == (SourceLoc{static_cast<uint32_t>(lines + 1), 1}));
}

TEST_CASE("file and string tokenization agree", "[lexer]") {
    const std::string path = std::string(C4_FIXTURE_DIR) + "/pretty.c";
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream.good());
    std::string source;
    source.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

    TokenizedSource from_string = tokenize_string(path, source);
    TokenizedSource from_file = tokenize_file(path);
    REQUIRE(from_string.tokens().size() == from_file.tokens().size());
    for (size_t i = 0; i < from_string.tokens().size(); ++i) {
        const Token& string_token = from_string.tokens()[i];
        const Token& file_token = from_file.tokens()[i];
        CHECK(string_token.kind == file_token.kind);
        CHECK(string_token.loc == file_token.loc);
        CHECK(*string_token.text == *file_token.text);
        CHECK(*string_token.file_name == *file_token.file_name);
    }
}

TEST_CASE("tokenizing a missing file reports the source diagnostic", "[lexer]") {
    ScopedCerrCapture capture;
    REQUIRE_THROWS_AS(tokenize_file("/nonexistent/c4-missing-input.c"), CompileError);
    REQUIRE(capture.text() ==
            "/nonexistent/c4-missing-input.c:1:1: error: cannot open input file\n");
}

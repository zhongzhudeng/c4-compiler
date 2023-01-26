#include "Parser.hpp"
#include "Lexer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxExternDecl.hpp"
#include "SyntaxStmt.hpp"
#include "fmt/core.h"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <utility>

class UnitTestParser {
public:
  std::string parseExpr(int idx, std::string src) {
    std::string filename = fmt::format("dummy{}.c", idx);
    SymbolInternalizer symbolInternalizer;
    Lexer lexer(filename.data(), std::move(src), symbolInternalizer);
    std::vector<Token> token_stream;
    lexer.tokenize([&](Token tok) { token_stream.emplace_back(tok); });
    Parser parser(std::move(token_stream));
    ExprPtr expr = parser.parseExpr();
    std::ostringstream os;
    expr->print(os);
    return os.str();
  }
  std::string parseDecl(int idx, std::string src) {
    std::string filename = fmt::format("dummy{}.c", idx);
    SymbolInternalizer symbolInternalizer;
    Lexer lexer(filename.data(), std::move(src), symbolInternalizer);
    std::vector<Token> token_stream;
    lexer.tokenize([&](Token tok) { token_stream.emplace_back(tok); });
    Parser parser(std::move(token_stream));
    DeclPtr decl = parser.parseDecl();
    std::ostringstream os;
    decl->print(os);
    return os.str();
  }
  std::string parseStmt(int idx, std::string src) {
    std::string filename = fmt::format("dummy{}.c", idx);
    SymbolInternalizer symbolInternalizer;
    Lexer lexer(filename.data(), std::move(src), symbolInternalizer);
    std::vector<Token> token_stream;
    lexer.tokenize([&](Token tok) { token_stream.emplace_back(tok); });
    Parser parser(std::move(token_stream));
    StmtPtr stmt = parser.parseStmt();
    std::ostringstream os;
    stmt->print(os);
    return os.str();
  }
};

TEST_CASE("Expr", "[Parser]") {
  UnitTestParser parser;
  SECTION("Primary") {
    REQUIRE(parser.parseExpr(1, "a") == "a");
    REQUIRE(parser.parseExpr(2, "1") == "1");
    REQUIRE(parser.parseExpr(3, R"("abc")") == R"("abc")");
    REQUIRE(parser.parseExpr(4, "(a)") == "a");
  }
  SECTION("PostfixExpr") {
    REQUIRE(parser.parseExpr(5, "f(a,b)") == "(f(a, b))");
    REQUIRE(parser.parseExpr(6, "f[a]") == "(f[a])");
    REQUIRE(parser.parseExpr(7, "s.x") == "(s.x)");
    REQUIRE(parser.parseExpr(8, "s->x") == "(s->x)");
  }
  SECTION("AssignmentExpr") {
    REQUIRE(parser.parseExpr(9, "a=b") == "(a = b)");
  }
  SECTION("ConditionalExpr") {
    REQUIRE(parser.parseExpr(9, "b=a==1?0:1") == "(b = ((a == 1) ? 0 : 1))");
  }
  SECTION("BinaryExpr") {
    REQUIRE(parser.parseExpr(10, "a+b") == "(a + b)");
    REQUIRE(parser.parseExpr(11, "a+b*c") == "(a + (b * c))");
    REQUIRE(parser.parseExpr(12, "a=b+1") == "(a = (b + 1))");
  }
  SECTION("UnaryExpr") { REQUIRE(parser.parseExpr(13, "!a") == "(!a)"); }
  SECTION("CombinedExpr") {
    REQUIRE(parser.parseExpr(14, "-f(1)") == "(-(f(1)))");
    REQUIRE(parser.parseExpr(15, "&s.x") == "(&(s.x))");
    REQUIRE(parser.parseExpr(16, "s.x=x") == "((s.x) = x)");
    REQUIRE(parser.parseExpr(17, "p=&s") == "(p = (&s))");
    REQUIRE(parser.parseExpr(18, "*&s.x") == "(*(&(s.x)))");
    REQUIRE(parser.parseExpr(19, "sizeof s.x * (p->x-y)") ==
            "((sizeof (s.x)) * ((p->x) - y))");
    REQUIRE(parser.parseExpr(20, "-(*&s.x = sizeof s.x * (p->x - y))") ==
            "(-((*(&(s.x))) = ((sizeof (s.x)) * ((p->x) - y))))");
    REQUIRE(parser.parseExpr(21, "a=1") == "(a = 1)");
    REQUIRE(parser.parseExpr(22, "b=a*2") == "(b = (a * 2))");
    REQUIRE(parser.parseExpr(23, "a<sizeof b") == "(a < (sizeof b))");
    REQUIRE(
        parser.parseExpr(24, "a < sizeof b || !(b == f(23, sizeof(int)))") ==
        "((a < (sizeof b)) || (!(b == (f(23, (sizeof(int)))))))");
    REQUIRE(parser.parseExpr(25, R"(c="bla")") == R"((c = "bla"))");
    REQUIRE(
        parser.parseExpr(26, "(a < sizeof b || !(b == f(23, sizeof(int))))") ==
        "((a < (sizeof b)) || (!(b == (f(23, (sizeof(int)))))))");
  }
}

TEST_CASE("Decl", "[Parser]") {
  UnitTestParser parser;
  SECTION("Struct") {
    REQUIRE(parser.parseDecl(0, "struct S{int x;} s") ==
            "struct S\n{\n\tint x;\n} s");
  }
  SECTION("Function") {
    REQUIRE(parser.parseDecl(0, "int main(void)") == "int (main(void))");
    REQUIRE(parser.parseDecl(0, "int (main(void))") == "int (main(void))");
    REQUIRE(parser.parseDecl(1, "int f(int x, int y)") ==
            "int (f(int x, int y))");
    REQUIRE(parser.parseDecl(1, "int (f(int x, int y))") ==
            "int (f(int x, int y))");
  }
  SECTION("Pointer") {
    REQUIRE(parser.parseDecl(0, "struct S *p") == "struct S (*p)");
    REQUIRE(parser.parseDecl(0, "struct S (*p)") == "struct S (*p)");
    REQUIRE(parser.parseDecl(1, "void *(**p)(int)") ==
            "void (*((*(*p))(int)))");
    REQUIRE(parser.parseDecl(1, "void (*((*(*p))(int)))") ==
            "void (*((*(*p))(int)))");
  }
}

TEST_CASE("Full", "[Parser]") {
  std::string src =
      R"(int (f(int x, int y))
{
	{
		struct S
		{
			int x;
		} s;
		((s.x) = x);
		struct S (*p);
		(p = (&s));
		if (1)
			return (-((*(&(s.x))) = ((sizeof (s.x)) * ((p->x) - y))));
		else if (1)
			return 1;
		else {
			return 0;
		}
	}
}

void (*((*(*p))(int)));

int (main(void))
{
	int a;
	int b;
	{
		(a = 1);
	}
	(b = (a * 2));
	if ((a < b)) {
		while (((a < (sizeof b)) || (!(b == (f(23, (sizeof(int))))))))
foo:
			break;
	} else if (0) {
		char (*c);
		while (1) {
			(c = "bla");
			continue;
		}
	} else
		goto end;
bar:
end:
	return 0;
}
)";
  std::string res = src;
  // REQUIRE(parser.parseExpr(0, src) == res);
}
#include "Parser.hpp"
#include "Lexer.hpp"
#include "Printer.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExpr.hpp"
#include "fmt/core.h"
#include <catch2/catch_test_macros.hpp>
#include <utility>
#include <sstream>

std::string testParseExpr(int idx, std::string src) {
  std::string filename = fmt::format("dummy{}.c", idx);
  SymbolInternalizer symbolInternalizer;
  Lexer lexer(filename.data(), std::move(src), symbolInternalizer);
  std::vector<Token> token_stream;
  lexer.tokenize([&](Token tok) { token_stream.emplace_back(tok); });
  Parser parser(std::move(token_stream));
  ASTPtr ast = parser.parse();
  std::ostringstream os;
  os << const_cast<AST &>(*ast);
  return os.str();
}

TEST_CASE("Expr", "[Parser]") {
  SECTION("Primary") {
    REQUIRE(testParseExpr(1, "a") == "a");
    REQUIRE(testParseExpr(2, "1") == "1");
    REQUIRE(testParseExpr(3, R"("abc")") == R"("abc")");
    REQUIRE(testParseExpr(4, "(a)") == "a");
  }
  SECTION("PostfixExpr") {
    REQUIRE(testParseExpr(5, "f(a,b)") == "(f(a, b))");
    REQUIRE(testParseExpr(6, "f[a]") == "(f[a])");
    REQUIRE(testParseExpr(7, "s.x") == "(s.x)");
    REQUIRE(testParseExpr(8, "s->x") == "(s->x)");
  }
  SECTION("AssignmentExpr") { REQUIRE(testParseExpr(9, "a=b") == "(a = b)"); }
  SECTION("ConditionalExpr") {
    REQUIRE(testParseExpr(9, "b=a==1?0:1") == "(b = ((a == 1) ? 0 : 1))");
  }
  SECTION("BinaryExpr") {
    REQUIRE(testParseExpr(10, "a+b") == "(a + b)");
    REQUIRE(testParseExpr(11, "a+b*c") == "(a + (b * c))");
    REQUIRE(testParseExpr(12, "a=b+1") == "(a = (b + 1))");
  }
  SECTION("UnaryExpr") { REQUIRE(testParseExpr(13, "!a") == "(!a)"); }
  SECTION("CombinedExpr") {
    REQUIRE(testParseExpr(14, "-f(1)") == "(-(f(1)))");
    REQUIRE(testParseExpr(15, "&s.x") == "(&(s.x))");
    REQUIRE(testParseExpr(16, "s.x=x") == "((s.x) = x)");
    REQUIRE(testParseExpr(17, "p=&s") == "(p = (&s))");
    REQUIRE(testParseExpr(18, "*&s.x") == "(*(&(s.x)))");
    REQUIRE(testParseExpr(19, "sizeof s.x * (p->x-y)") ==
            "((sizeof (s.x)) * ((p->x) - y))");
    REQUIRE(testParseExpr(20, "-(*&s.x = sizeof s.x * (p->x - y))") ==
            "(-((*(&(s.x))) = ((sizeof (s.x)) * ((p->x) - y))))");
    REQUIRE(testParseExpr(21, "a=1") == "(a = 1)");
    REQUIRE(testParseExpr(22, "b=a*2") == "(b = (a * 2))");
    REQUIRE(testParseExpr(23, "a<sizeof b") == "(a < (sizeof b))");
    REQUIRE(testParseExpr(24, "a < sizeof b || !(b == f(23, sizeof(int)))") ==
            "((a < (sizeof b)) || (!(b == (f(23, (sizeof(int)))))))");
    REQUIRE(testParseExpr(25, R"(c="bla")") == R"((c = "bla"))");
    REQUIRE(testParseExpr(26, "(a < sizeof b || !(b == f(23, sizeof(int))))") ==
            "((a < (sizeof b)) || (!(b == (f(23, (sizeof(int)))))))");
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
  REQUIRE(testParseExpr(0, src) == res);
}
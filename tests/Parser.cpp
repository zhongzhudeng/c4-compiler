#include "Parser.hpp"
#include "SyntaxDecl.hpp"
#include "UnitTestParser.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Expr", "[Parser]") {
  UnitTestParser<Expr> parser;
  SECTION("Primary") {
    REQUIRE(parser.parse(1, "a") == "a");
    REQUIRE(parser.parse(2, "1") == "1");
    REQUIRE(parser.parse(3, R"("abc")") == R"("abc")");
    REQUIRE(parser.parse(4, "(a)") == "a");
  }
  SECTION("PostfixExpr") {
    REQUIRE(parser.parse(5, "f(a,b)") == "(f(a, b))");
    REQUIRE(parser.parse(6, "f[a]") == "(f[a])");
    REQUIRE(parser.parse(7, "s.x") == "(s.x)");
    REQUIRE(parser.parse(8, "s->x") == "(s->x)");
  }
  SECTION("AssignmentExpr") { REQUIRE(parser.parse(9, "a=b") == "(a = b)"); }
  SECTION("ConditionalExpr") {
    REQUIRE(parser.parse(9, "b=a==1?0:1") == "(b = ((a == 1) ? 0 : 1))");
  }
  SECTION("BinaryExpr") {
    REQUIRE(parser.parse(10, "a+b") == "(a + b)");
    REQUIRE(parser.parse(11, "a+b*c") == "(a + (b * c))");
    REQUIRE(parser.parse(12, "a=b+1") == "(a = (b + 1))");
  }
  SECTION("UnaryExpr") { REQUIRE(parser.parse(13, "!a") == "(!a)"); }
  SECTION("CombinedExpr") {
    REQUIRE(parser.parse(14, "-f(1)") == "(-(f(1)))");
    REQUIRE(parser.parse(15, "&s.x") == "(&(s.x))");
    REQUIRE(parser.parse(16, "s.x=x") == "((s.x) = x)");
    REQUIRE(parser.parse(17, "p=&s") == "(p = (&s))");
    REQUIRE(parser.parse(18, "*&s.x") == "(*(&(s.x)))");
    REQUIRE(parser.parse(19, "sizeof s.x * (p->x-y)") ==
            "((sizeof (s.x)) * ((p->x) - y))");
    REQUIRE(parser.parse(20, "-(*&s.x = sizeof s.x * (p->x - y))") ==
            "(-((*(&(s.x))) = ((sizeof (s.x)) * ((p->x) - y))))");
    REQUIRE(parser.parse(21, "a=1") == "(a = 1)");
    REQUIRE(parser.parse(22, "b=a*2") == "(b = (a * 2))");
    REQUIRE(parser.parse(23, "a<sizeof b") == "(a < (sizeof b))");
    REQUIRE(parser.parse(24, "a < sizeof b || !(b == f(23, sizeof(int)))") ==
            "((a < (sizeof b)) || (!(b == (f(23, (sizeof(int)))))))");
    REQUIRE(parser.parse(25, R"(c="bla")") == R"((c = "bla"))");
    REQUIRE(parser.parse(26, "(a < sizeof b || !(b == f(23, sizeof(int))))") ==
            "((a < (sizeof b)) || (!(b == (f(23, (sizeof(int)))))))");
    REQUIRE(parser.parse(
                26, "((a < (sizeof b)) || (!(b == (f(23, (sizeof(int)))))))") ==
            "((a < (sizeof b)) || (!(b == (f(23, (sizeof(int)))))))");
  }
}

TEST_CASE("Decl", "[Parser]") {
  UnitTestParser<DeclaratorDecl> parser;
  SECTION("Struct") {
    REQUIRE(parser.parse(0, "struct S{int x;} s") ==
            "struct S\n{\n\tint x;\n} s");
  }
  SECTION("FunctionVariable") {
    REQUIRE(parser.parse(1, "void *(**p)(int)") == "void (*((*(*p))(int)))");
    REQUIRE(parser.parse(1, "void (*((*(*p))(int)))") ==
            "void (*((*(*p))(int)))");
  }
  SECTION("Function") {
    REQUIRE(parser.parse(0, "int main(void)") == "int (main(void))");
    REQUIRE(parser.parse(0, "int (main(void))") == "int (main(void))");
    REQUIRE(parser.parse(1, "int f(int x, int y)") == "int (f(int x, int y))");
    REQUIRE(parser.parse(1, "int (f(int x, int y))") ==
            "int (f(int x, int y))");
    REQUIRE(parser.parse(2, "int f(int x, void *(**p)(int))") ==
            "int (f(int x, void (*((*(*p))(int)))))");
    // TODO: anoymous function variable in parameter
    // REQUIRE(parser.parse(2, "int f(int x, void *(**)(int))") ==
    // "int (f(int x, int y))");
  }
  SECTION("Pointer") {
    REQUIRE(parser.parse(0, "struct S{int x;} *p") ==
            "struct S\n{\n\tint x;\n} (*p)");
    REQUIRE(parser.parse(0, "struct S{int x;} **p") ==
            "struct S\n{\n\tint x;\n} (*(*p))");
  }
}

TEST_CASE("Stmt", "[Parser]") {
  UnitTestParser<Stmt> parser;
  SECTION("ExprStmt") { REQUIRE(parser.parse(0, "a = 1;") == "(a = 1);"); }
  SECTION("CompStmt") {
    REQUIRE(parser.parse(0, "{a=1;b=1;}") == "{\n\t(a = 1);\n\t(b = 1);\n}");
  }
  SECTION("ReturnStmt") {
    REQUIRE(parser.parse(0, "return 0;") == "return 0;");
    REQUIRE(parser.parse(1, "return -(*&s.x = sizeof s.x * (p->x - y));") ==
            "return (-((*(&(s.x))) = ((sizeof (s.x)) * ((p->x) - y))));");
  }
  SECTION("IfStmt") {
    REQUIRE(
        parser.parse(0, "if (1) return -(*&s.x = sizeof s.x * (p->x - y));") ==
        "if (1)\n\treturn (-((*(&(s.x))) = ((sizeof (s.x)) * ((p->x) - y))));");
  }
  SECTION("IfElseStmt") {
    REQUIRE(parser.parse(0, "if (1) return -(*&s.x = sizeof s.x * (p->x - y)); "
                            "else return 0;") ==
            "if (1)\n\treturn (-((*(&(s.x))) = ((sizeof (s.x)) * ((p->x) - "
            "y))));\nelse\n\treturn 0;");
  }
  SECTION("IfElseIf") {
    REQUIRE(parser.parse(0, "if (1) return -(*&s.x = sizeof s.x * (p->x - y)); "
                            "else if (1) return 1;else return 0;") ==
            "if (1)\n\treturn (-((*(&(s.x))) = ((sizeof (s.x)) * ((p->x) - "
            "y))));\nelse if (1)\n\treturn 1;\nelse\n\treturn 0;");
  }
  SECTION("WhileStmt") {
    REQUIRE(parser.parse(0, "if (a < b) {while (1)foo:break;}") ==
            "if ((a < b)) {\n\twhile (1)\nfoo:\n\t\tbreak;\n}");
    REQUIRE(parser.parse(1, "if ((a < b)) {while (((a < (sizeof b)) || (!(b == "
                            "(f(23, (sizeof(int))))))))foo:break;}") ==
            "if ((a < b)) {\n\twhile (((a < (sizeof b)) || (!(b == "
            "(f(23, (sizeof(int))))))))\nfoo:\n\t\tbreak;\n}");
  }
}

TEST_CASE("ExternalDeclaration", "[Parser]") {
  UnitTestParser<Decl> parser;
  SECTION("FuncDef") {
    REQUIRE(parser.parse(0, "int main(void){return 0;}") ==
            "int (main(void))\n{\n\treturn 0;\n}\n");
    REQUIRE(parser.parse(1, "int main(void){int a;int b;}") ==
            "int (main(void))\n{\n\tint a;\n\tint b;\n}\n");
  }
}
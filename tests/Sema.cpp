#include "Compiler.hpp"
#include "UnitTestParser.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("ExternalDeclaration", "[Parser]") {
  UnitTestParser parser;
  SECTION("main") {
    REQUIRE(parser.parse("int main(void){return 0;}") ==
            "int (main(void))\n{\n\treturn 0;\n}\n");
    REQUIRE(parser.parse("int (main(void)){return 0;}") ==
            "int (main(void))\n{\n\treturn 0;\n}\n");
  }
  SECTION("FuncDef") {
    REQUIRE(parser.parse("int f(void); int f(void){return 0;}") ==
            "int (f(void));\nint (f(void))\n{\n\treturn 0;\n}\n");
    REQUIRE(parser.parse("int f(void){int a;int b;}") ==
            "int (f(void))\n{\n\tint a;\n\tint b;\n}\n");
    REQUIRE_THROWS(
        parser.parse("int f(void){return 0;} int f(void){return 0;}"));
  }
  SECTION("Function") {
    REQUIRE(parser.parse("int f(void);") == "int (f(void));");
    REQUIRE(parser.parse("int (f(void));") == "int (f(void));");
    REQUIRE(parser.parse("int f(int x, int y);") == "int (f(int x, int y));");
    REQUIRE(parser.parse("int (f(int x, int y));") == "int (f(int x, int y));");
    REQUIRE(parser.parse("int f(int x, void *(**p)(int));") ==
            "int (f(int x, void (*((*(*p))(int)))));");
    REQUIRE(parser.parse("int *f(int x, int y);") ==
            "int (*(f(int x, int y)));");
    // TODO: add abstract function variable support
    // REQUIRE(parser.parse("int f(int x, void *(**)(int))") ==
    // "int (f(int x, void (*((*(*))(int)))))");
  }
  SECTION("Declaration") {
    REQUIRE(parser.parse("int a;") == "int a;");
    REQUIRE(parser.parse("struct S{int x;};") == "struct S\n{\n\tint x;\n};");
    REQUIRE(parser.parse("struct S{int x;} s;") ==
            "struct S\n{\n\tint x;\n} s;");
  }
}

TEST_CASE("Decl", "[Parser]") {
  UnitTestParser parser;
  SECTION("Struct") {
    REQUIRE(parser.parse("struct S{int x;} s;") ==
            "struct S\n{\n\tint x;\n} s;");
  }
  SECTION("FunctionVariable") {
    REQUIRE(parser.parse("void *(**p)(int);") == "void (*((*(*p))(int)));");
    REQUIRE(parser.parse("void (*((*(*p))(int)));") ==
            "void (*((*(*p))(int)));");
  }
  SECTION("Pointer") {
    REQUIRE(parser.parse("struct S{int x;} *p;") ==
            "struct S\n{\n\tint x;\n} (*p);");
    REQUIRE(parser.parse("struct S{int x;} **p;") ==
            "struct S\n{\n\tint x;\n} (*(*p));");
  }
}

// TEST_CASE("Expr", "[Sema]") {
//   UnitTestParser<Expr> parser;
//   SECTION("example") { REQUIRE_THROWS(parser.parse(0, "a || b =c")); }
// }

// TEST_CASE("Decl", "[Sema]") {
// REQUIRE(parser.parse(0, "struct S *p") == "struct S (*p)");
// TODO: definition
// REQUIRE(parser.parse(0, "struct S (*p)") == "struct S (*p)");
// }

// TEST_CASE("Stmt", "[Sema]") {
//   UnitTestParser<Stmt> parser;
//   SECTION("Label") { REQUIRE_THROWS(parser.parse(0, "a || b =c")); }
//   SECTION("goto") { REQUIRE_THROWS(parser.parse(0, "a || b =c")); }
// }

TEST_CASE("Type", "[Parser]") {
  UnitTestParser parser;
  SECTION("return") {
    REQUIRE_THROWS(parser.parse("int main(void){return \"abc\";}"));
  }
}
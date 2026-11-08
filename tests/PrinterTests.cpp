#include "TestSupport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

namespace {

std::string readFixture(const std::string &name) {
  std::ifstream file(std::string(C4_FIXTURE_DIR) + "/" + name,
                     std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("printer matches the reference pretty file", "[printer]") {
  TestCompiler compiler;
  std::string source = readFixture("pretty.c");
  std::string output;
  std::string error;
  INFO(error);
  REQUIRE(compiler.print(source, &output, &error));
  REQUIRE(output == source);
}

TEST_CASE("printer formats a minimal function", "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print("int main(void) { return 0; }", &output));
  REQUIRE(output == "int (main(void))\n{\n\treturn 0;\n}\n");
}

TEST_CASE("printer fully parenthesizes declarators", "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print(
      "void *(**p)(int);\nint *f(int x, int y);\nint (*g)(int);\n",
      &output));
  REQUIRE(output ==
          "void (*((*(*p))(int)));\n"
          "\n"
          "int (*(f(int x, int y)));\n"
          "\n"
          "int ((*g)(int));\n");
}

TEST_CASE("printer formats control flow and labels", "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print(
      "int main(void){int a; if(a){a=1;} else if(a) a=2; else {a=3;} "
      "while(a){break;} goto end; end: return a;}",
      &output));
  REQUIRE(output ==
          "int (main(void))\n"
          "{\n"
          "\tint a;\n"
          "\tif (a) {\n"
          "\t\t(a = 1);\n"
          "\t} else if (a)\n"
          "\t\t(a = 2);\n"
          "\telse {\n"
          "\t\t(a = 3);\n"
          "\t}\n"
          "\twhile (a) {\n"
          "\t\tbreak;\n"
          "\t}\n"
          "\tgoto end;\n"
          "end:\n"
          "\treturn a;\n"
          "}\n");
}

TEST_CASE("printer formats sizeof expressions", "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print(
      "int main(void){return sizeof(int *) + sizeof \"abc\";}", &output));
  REQUIRE(output ==
          "int (main(void))\n"
          "{\n"
          "\treturn ((sizeof(int *)) + (sizeof \"abc\"));\n"
          "}\n");
}

TEST_CASE("printer renders directly from the AST", "[printer][architecture]") {
  TestCompiler compiler;
  REQUIRE(compiler.parse("int main(void) { return 0; }"));
  std::ostringstream stream;
  Printer printer(compiler.ast(), stream);
  printer.print();
  REQUIRE(stream.str() == "int (main(void))\n{\n\treturn 0;\n}\n");
}

TEST_CASE("printer expands comma-separated struct members", "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print("struct S { int a, b; char c; } s;", &output));
  REQUIRE(output ==
          "struct S\n"
          "{\n"
          "\tint a;\n"
          "\tint b;\n"
          "\tchar c;\n"
          "} s;\n");
}

TEST_CASE("printer keeps struct declarations inline in type-name contexts",
          "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print(
      "int f(struct S { int a, b; } p);\n"
      "int main(void) { return sizeof(struct T { int x, y; }); }\n",
      &output));
  REQUIRE(output ==
          "int (f(struct S { int a; int b; } p));\n"
          "\n"
          "int (main(void))\n"
          "{\n"
          "\treturn (sizeof(struct T { int x; int y; }));\n"
          "}\n");
}

TEST_CASE("printer nests multiline struct declarations", "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print(
      "struct Outer { struct Inner { int x; } inner; int y; } o;", &output));
  REQUIRE(output ==
          "struct Outer\n"
          "{\n"
          "\tstruct Inner\n"
          "\t{\n"
          "\t\tint x;\n"
          "\t} inner;\n"
          "\tint y;\n"
          "} o;\n");
}

TEST_CASE("printer canonicalizes digraph braces", "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print("int main(void) <% return 0; %>", &output));
  REQUIRE(output == "int (main(void))\n{\n\treturn 0;\n}\n");
  REQUIRE(output.find("<%") == std::string::npos);
  REQUIRE(output.find("%>") == std::string::npos);
}

TEST_CASE("printer formats prototypes and definitions consistently",
          "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print(
      "int f(int x);\nint f(int x) { return x; }\n", &output));
  REQUIRE(output ==
          "int (f(int x));\n"
          "\n"
          "int (f(int x))\n"
          "{\n"
          "\treturn x;\n"
          "}\n");
}

TEST_CASE("printer fully parenthesizes nested expressions", "[printer]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print(
      "int main(void) { return 1 + 2 * 3 == 4 && !0; }", &output));
  REQUIRE(output ==
          "int (main(void))\n"
          "{\n"
          "\treturn (((1 + (2 * 3)) == 4) && (!0));\n"
          "}\n");
}

TEST_CASE("printer output is idempotent", "[printer]") {
  TestCompiler compiler;
  std::string first;
  REQUIRE(compiler.print(
      "struct S { int a, b; } s; "
      "int f(int x) { if (x) { return x; } return 0; }",
      &first));
  std::string second;
  REQUIRE(compiler.print(first, &second));
  REQUIRE(second == first);
}

#include "TestSupport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
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

#include <catch2/catch_test_macros.hpp>

#include "TestSupport.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string compileOrFail(const std::string &source, bool optimize = false) {
  TestCompiler compiler;
  std::string ir;
  std::string error;
  REQUIRE(compiler.compile(source, optimize, &ir, &error));
  return ir;
}

bool contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("codegen emits a constant return", "[codegen]") {
  std::string ir = compileOrFail("int main(void) { return 42; }");
  REQUIRE(contains(ir, "define i32 @main()"));
  REQUIRE(contains(ir, "ret i32 42"));
}

TEST_CASE("codegen emits function calls", "[codegen]") {
  std::string ir = compileOrFail(R"(
int add(int a, int b) { return a + b; }
int main(void) { return add(40, 2); }
)");
  REQUIRE(contains(ir, "define i32 @add(i32"));
  REQUIRE(contains(ir, "call i32 @add(i32 40, i32 2)"));
  REQUIRE(contains(ir, "ret i32 %"));
}

TEST_CASE("codegen emits globals with zero initializers", "[codegen]") {
  std::string ir = compileOrFail("int g; char c; int main(void) { return g; }");
  REQUIRE(contains(ir, "@g = global i32 0"));
  REQUIRE(contains(ir, "@c = global i8 0"));
}

TEST_CASE("codegen promotes char loads", "[codegen]") {
  std::string ir = compileOrFail("int main(void) { char c; c = 'A'; return c; }");
  REQUIRE(contains(ir, "alloca i8"));
  REQUIRE(contains(ir, "load i8"));
  REQUIRE(contains(ir, "sext i8"));
}

TEST_CASE("codegen lowers struct member accesses to GEPs", "[codegen]") {
  std::string ir = compileOrFail(R"(
struct S { int a; char b; };
int main(void) { struct S s; s.a = 1; s.b = 'x'; return s.a; }
)");
  REQUIRE(contains(ir, "alloca { i32, i8 }"));
  REQUIRE(contains(ir, "getelementptr inbounds nuw { i32, i8 }"));
}

TEST_CASE("codegen materializes string literals", "[codegen]") {
  std::string ir = compileOrFail(R"(
int main(void) { char *s; s = "hi"; return *s; }
)");
  REQUIRE(contains(ir, "private constant [3 x i8]"));
  REQUIRE(contains(ir, "c\"hi\\00\""));
}

TEST_CASE("codegen scales pointer arithmetic", "[codegen]") {
  std::string ir = compileOrFail(R"(
int main(void) { int x; int *p; p = &x; return *(p + 1); }
)");
  REQUIRE(contains(ir, "getelementptr i32, ptr"));
}

TEST_CASE("codegen lowers goto and labels", "[codegen]") {
  std::string ir = compileOrFail(R"(
int main(void) { int x; x = 0; goto end; x = 1; end: return x; }
)");
  REQUIRE(contains(ir, "br label %end"));
  REQUIRE(contains(ir, "end:"));
}

TEST_CASE("codegen lowers conditional expressions to PHIs", "[codegen]") {
  std::string ir = compileOrFail(R"(
int main(int a) { int x; x = a ? 1 : 2; return x; }
)");
  REQUIRE(contains(ir, "cond.then"));
  REQUIRE(contains(ir, "cond.else"));
  REQUIRE(contains(ir, "phi i32"));
}

TEST_CASE("codegen short-circuits logical operators", "[codegen]") {
  std::string ir = compileOrFail(R"(
int main(int a) { int x; x = a && (a - 1); return x; }
)");
  REQUIRE(contains(ir, "logic.rhs"));
  REQUIRE(contains(ir, "phi i1"));
}

TEST_CASE("codegen emits void returns", "[codegen]") {
  std::string ir = compileOrFail("void f(void) { return; } int main(void) { f(); return 0; }");
  REQUIRE(contains(ir, "define void @f()"));
  REQUIRE(contains(ir, "ret void"));
}

TEST_CASE("codegen allocates every parameter in the entry block", "[codegen]") {
  std::string ir = compileOrFail("int f(int a, char b) { return a + b; }");
  REQUIRE(contains(ir, "alloca i32"));
  REQUIRE(contains(ir, "alloca i8"));
  REQUIRE(contains(ir, "store i32 %a1, ptr"));
  REQUIRE(contains(ir, "store i8 %b2, ptr"));
}

TEST_CASE("codegen hoists locals declared in nested blocks", "[codegen]") {
  std::string ir = compileOrFail(R"(
int main(void) {
  int x;
  x = 1;
  if (x) {
    char *c;
    c = "nested";
    x = *c;
  }
  return x;
}
)");
  REQUIRE(contains(ir, "alloca ptr"));
  REQUIRE(contains(ir, "if.then"));
}

TEST_CASE("codegen accepts the reference pretty fixture", "[codegen]") {
  std::ifstream stream(std::string(C4_FIXTURE_DIR) + "/pretty.c");
  REQUIRE(stream.good());
  std::stringstream buffer;
  buffer << stream.rdbuf();

  TestCompiler compiler;
  std::string ir;
  std::string error;
  REQUIRE(compiler.compile(buffer.str(), /*optimize=*/false, &ir, &error));
  REQUIRE(contains(ir, "define i32 @main"));
}

TEST_CASE("codegen coerces conditional arms in their predecessor blocks",
          "[codegen]") {
  const std::string thenArm =
      "int f(int x) { char c; c = 1; return x ? c : 2; }";
  std::string ir = compileOrFail(thenArm);
  size_t thenBlock = ir.find("cond.then:");
  size_t thenSext = ir.find("sext i8", thenBlock);
  size_t thenBranch = ir.find("br label %cond.end", thenBlock);
  REQUIRE(thenBlock != std::string::npos);
  REQUIRE(thenSext != std::string::npos);
  REQUIRE(thenBranch != std::string::npos);
  REQUIRE(thenSext < thenBranch);

  const std::string elseArm =
      "int f(int x) { char c; c = 3; return x ? 2 : c; }";
  ir = compileOrFail(elseArm);
  size_t elseBlock = ir.find("cond.else:");
  size_t elseSext = ir.find("sext i8", elseBlock);
  size_t elseBranch = ir.find("br label %cond.end", elseBlock);
  REQUIRE(elseBlock != std::string::npos);
  REQUIRE(elseSext != std::string::npos);
  REQUIRE(elseBranch != std::string::npos);
  REQUIRE(elseSext < elseBranch);

  const std::string nested =
      "int f(int x) { char c; c = 4; "
      "return x ? (x ? c : 2) : c; }";
  compileOrFail(nested);
  compileOrFail(nested, /*optimize=*/true);
}

#include <catch2/catch_test_macros.hpp>

#include "TestSupport.hpp"

#include <string>

namespace {

std::string optimizeOrFail(const std::string &source) {
  TestCompiler compiler;
  std::string ir;
  std::string error;
  REQUIRE(compiler.compile(source, /*optimize=*/true, &ir, &error));
  return ir;
}

bool contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("mem2reg promotes local scalars to SSA", "[sccp]") {
  std::string ir = optimizeOrFail("int f(int a) { int x; x = a + 1; return x; }");
  REQUIRE_FALSE(contains(ir, "alloca"));
  REQUIRE_FALSE(contains(ir, "load i32"));
}

TEST_CASE("SCCP folds a constant conditional branch", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int main(void) {
  int x;
  x = 1;
  if (1) { x = 2; } else { x = 3; }
  return x;
}
)");
  REQUIRE(contains(ir, "ret i32 2"));
  REQUIRE_FALSE(contains(ir, "if.else"));
  REQUIRE_FALSE(contains(ir, "icmp"));
}

TEST_CASE("SCCP removes an unreachable then-branch", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int f(void) { if (0) { return 1; } return 2; }
)");
  REQUIRE(contains(ir, "ret i32 2"));
  REQUIRE_FALSE(contains(ir, "ret i32 1"));
  REQUIRE_FALSE(contains(ir, "if.then"));
}

TEST_CASE("SCCP removes a while loop with a constant false condition", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int main(void) { int x; x = 1; while (0) { x = x + 1; } return x; }
)");
  REQUIRE(contains(ir, "ret i32 1"));
  REQUIRE_FALSE(contains(ir, "while.body"));
}

TEST_CASE("SCCP propagates constants through arithmetic", "[sccp]") {
  std::string ir = optimizeOrFail("int main(void) { int x; x = 3 * 4 + 1; return x; }");
  REQUIRE(contains(ir, "ret i32 13"));
}

TEST_CASE("SCCP folds PHIs with equal constant inputs", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int f(int a) { int x; if (a) { x = 1; } else { x = 1; } return x; }
)");
  REQUIRE(contains(ir, "ret i32 1"));
  REQUIRE_FALSE(contains(ir, "phi i32"));
}

TEST_CASE("SCCP keeps a branch whose condition is unknown", "[sccp]") {
  std::string ir = optimizeOrFail("int f(int a) { if (a) { return 1; } return 2; }");
  REQUIRE(contains(ir, "br i1"));
  REQUIRE(contains(ir, "ret i32 1"));
  REQUIRE(contains(ir, "ret i32 2"));
}

TEST_CASE("SCCP preserves loops with unknown bounds", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int f(int n) {
  int s;
  s = 0;
  while (n) {
    s = s + n;
    n = n - 1;
  }
  return s;
}
)");
  REQUIRE(contains(ir, "while.cond"));
  REQUIRE(contains(ir, "phi i32"));
  REQUIRE(contains(ir, "br i1"));
}

TEST_CASE("SCCP removes dead instructions after folding", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int main(void) { int x; x = 1; if (x) { x = 2; } return x; }
)");
  REQUIRE(contains(ir, "ret i32 2"));
  REQUIRE_FALSE(contains(ir, "icmp"));
}

TEST_CASE("SCCP keeps a live PHI after folding branches", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int f(int a) { int x; if (1) { x = 7; } else { x = a; } return x; }
)");
  REQUIRE(contains(ir, "ret i32 7"));
}

TEST_CASE("SCCP keeps blocks reachable through goto", "[sccp]") {
  std::string ir = optimizeOrFail(R"(
int main(void) {
  int x;
  x = 0;
  if (1) {
    x = 1;
  } else {
  target:
    return 2;
  }
  goto target;
  return x;
}
)");
  REQUIRE(contains(ir, "target:"));
  REQUIRE(contains(ir, "ret i32 2"));
  REQUIRE_FALSE(contains(ir, "if.else"));
}

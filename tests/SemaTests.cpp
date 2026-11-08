#include "TestSupport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

void requireValid(const std::string &source) {
  TestCompiler compiler;
  std::string error;
  INFO(source);
  REQUIRE(compiler.analyze(source, &error));
}

void requireInvalid(const std::string &source) {
  TestCompiler compiler;
  std::string error;
  INFO(source);
  REQUIRE_FALSE(compiler.analyze(source, &error));
  REQUIRE(error.find("test.c:") != std::string::npos);
  REQUIRE(error.find(": error: ") != std::string::npos);
}

} // namespace

TEST_CASE("sema resolves names, types, pointers, and calls", "[sema]") {
  requireValid("int main(void) { return 0; }");
  requireValid("int f(int x, int y) { return x + y; } "
               "int main(void) { return f(1, 2); }");
  requireValid("int main(void) { int *p; int i; p = &i; *p = 1; "
               "return i; }");
  requireValid("int main(void) { int *p; p = 0; return p == 0; }");
  requireValid("int main(void) { char *p; p = \"abc\"; return p[0]; }");
  requireValid("int main(void) { int *p; void *v; p = v; v = p; "
               "return p == v; }");
  requireValid("int main(void) { int *p; int *q; return p < q; }");
  requireValid("int main(void) { int *p; int *q; return p - q; }");
  requireValid("int f(int x) { return x; } "
               "int main(void) { int (*p)(int); p = f; return p(1); }");
  requireValid("int f(int x) { return x; } "
               "int main(void) { int (*p)(int); p = &f; return (*p)(1); }");
  requireValid("int main(void) { int (*p)(void); p = 0; return p == 0; }");
  requireValid("int main(void) { int a; int b; int c; a = b = c = 1; "
               "return a; }");
  requireValid("int main(void) { int a; a = 1 ? 2 : 3; return a; }");
  requireValid("void f(void) {} int main(void) { 1 ? f() : f(); "
               "return 0; }");
  requireValid("int main(void) { int x; x = sizeof(int); "
               "x = sizeof(int *); x = sizeof \"abc\"; return x; }");
}

TEST_CASE("sema handles structs and tags", "[sema]") {
  requireValid("struct S { int x; int y; }; "
               "int main(void) { struct S s; struct S *p; p = &s; "
               "p->x = 1; (*p).y = 2; return s.x + s.y; }");
  requireValid("struct S { struct S *next; int x; }; "
               "int main(void) { struct S a; struct S b; a.next = &b; "
               "a.next->x = 3; return a.next->x; }");
  requireValid("struct S; struct S *p; struct S { int x; }; "
               "int main(void) { struct S s; p = &s; p->x = 4; "
               "return p->x; }");
  requireValid("int main(void) { struct S { int x; } s; s.x = 1; "
               "return s.x; }");
  requireValid("int main(void) { struct { int x; } s; s.x = 1; "
               "return s.x; }");
  requireValid("struct S { int x; }; "
               "void f(struct S *p) { p->x = 1; } "
               "int main(void) { struct S s; f(&s); return s.x; }");
}

TEST_CASE("sema handles labels, loops, and returns", "[sema]") {
  requireValid("int main(void) { goto end; end: return 0; }");
  requireValid("int main(void) { goto end; { end: ; } return 0; }");
  requireValid("int main(void) { a: b: return 0; }");
  requireValid("int main(void) { while (1) { if (1) break; "
               "else continue; } return 0; }");
  requireValid("void f(void) { return; } int main(void) { f(); "
               "return 0; }");
  requireValid("int main(void) { int x; { int x; x = 1; } return x; }");
  requireValid("int f(int x) { { int x; x = 1; } return x; } "
               "int main(void) { return f(0); }");
}

TEST_CASE("sema rejects invalid programs", "[sema]") {
  requireInvalid("int main(void) { return x; }");
  requireInvalid("int main(void) { int x; int x; return 0; }");
  requireInvalid("int f(int); int f(char); int main(void) { return 0; }");
  requireInvalid("int f(void) { return 0; } int f(void) { return 1; } "
                 "int main(void) { return 0; }");
  requireInvalid("int main(void) { int a; int b; a + b = 1; return 0; }");
  requireInvalid("int f(void) { return 1; } "
                 "int main(void) { f() = 1; return 0; }");
  requireInvalid("int main(void) { \"abc\" = \"def\"; return 0; }");
  requireInvalid("int f(void) { return 1; } "
                 "int main(void) { f = 0; return 0; }");
  requireInvalid("int main(void) { int *p; char *q; p = q; return 0; }");
  requireInvalid("int main(void) { int *p; p = 1; return 0; }");
  requireInvalid("int main(void) { void *p; p = p + 1; return 0; }");
  requireInvalid("int main(void) { int (*p)(void); p = p + 1; return 0; }");
  requireInvalid("int main(void) { int x; x = *x; return 0; }");
  requireInvalid("int main(void) { void *p; *p; return 0; }");
  requireInvalid("int main(void) { int x; x(); return 0; }");
  requireInvalid("int f(int x) { return x; } "
                 "int main(void) { return f(); }");
  requireInvalid("int f(int *p) { return 0; } "
                 "int main(void) { int x; return f(x); }");
  requireInvalid("int main(void) { int x; return x.y; }");
  requireInvalid("struct S { int x; }; "
                 "int main(void) { struct S s; return s.y; }");
  requireInvalid("struct S { int x; }; "
                 "int main(void) { struct S s; return s->x; }");
  requireInvalid("struct S { int x; }; "
                 "int main(void) { struct S *p; return p.x; }");
  requireInvalid("int main(void) { return sizeof(void); }");
  requireInvalid("int f(void) { return 0; } "
                 "int main(void) { return sizeof(f); }");
  requireInvalid("struct S; int main(void) { return sizeof(struct S); }");
  requireInvalid("struct S { int x; }; "
                 "int main(void) { struct S s; if (s) return 1; "
                 "return 0; }");
  requireInvalid("void f(void) {} int main(void) { if (f()) return 1; "
                 "return 0; }");
  requireInvalid("int main(void) { break; return 0; }");
  requireInvalid("int main(void) { continue; return 0; }");
  requireInvalid("int main(void) { goto nowhere; return 0; }");
  requireInvalid("int main(void) { a: a: return 0; }");
  requireInvalid("void f(void) { return 1; } int main(void) { return 0; }");
  requireInvalid("int f(void) { return; } int main(void) { return 0; }");
  requireInvalid("int f(void) { return \"x\"; } "
                 "int main(void) { return 0; }");
  requireInvalid("int main(void) { void x; return 0; }");
  requireInvalid("int f(void)(void); int main(void) { return 0; }");
  requireInvalid("struct S { int x; }; struct S f(void); "
                 "int main(void) { return 0; }");
  requireInvalid("struct S { int x; }; void f(struct S s); "
                 "int main(void) { return 0; }");
  requireInvalid("struct S; int main(void) { struct S s; return 0; }");
  requireInvalid("struct S { struct S s; }; int main(void) { return 0; }");
  requireInvalid("struct S { int x; int x; }; int main(void) { return 0; }");
  requireInvalid("struct S { int x; }; struct S { int y; }; "
                 "int main(void) { return 0; }");
  requireInvalid("struct S { }; int main(void) { return 0; }");
  requireInvalid("void f(void x); int main(void) { return 0; }");
  requireInvalid("int f(int x, int x); int main(void) { return 0; }");
  requireInvalid("int f(int) { return 0; } int main(void) { return 0; }");
  requireInvalid("int main(void) { int f(void) { return 0; } return 0; }");
  requireInvalid("int x; char x; int main(void) { return 0; }");
  requireInvalid("int f(void); int f; int main(void) { return 0; }");
  requireInvalid("int x; int x(void); int main(void) { return 0; }");
  requireInvalid("int main(void) { return 2147483648; }");
  requireInvalid("int main(void) { int *p; char *q; int x; "
                 "p = x ? p : q; return 0; }");
  requireInvalid("struct S { int x; }; "
                 "int main(void) { struct S a; struct S b; a = b; "
                 "return 0; }");
  requireInvalid("int main(void) { int *p; int *q; p = p + q; return 0; }");
  requireInvalid("int main(void) { void *p; void *q; int x; x = p - q; "
                 "return 0; }");
  requireInvalid("int main(void) { void *p; void *q; int x; x = p < q; "
                 "return 0; }");
  requireInvalid("int f(void) { return 0; } "
                 "int main(void) { void *p; int x; x = f == p; "
                 "return 0; }");
  requireInvalid("struct S; int main(void) { struct S *p; struct S *q; "
                 "int x; x = p - q; return 0; }");
  requireInvalid("struct S; int main(void) { struct S *p; p = p + 1; "
                 "return 0; }");
  requireInvalid("struct S; int main(void) { struct S *p; return p->x; }");
  requireInvalid("int main(void) { int *p; int x; x = p < 0; return 0; }");
  requireInvalid("int main(void) { int *p; char *q; int x; x = p == q; "
                 "return 0; }");
  requireInvalid("void f(void) {} int main(void) { f() ? 1 : 2; "
                 "return 0; }");
  requireInvalid("int main(void) { int x; x = &(x + 1); return 0; }");
  requireInvalid("int main(void) { int *p; p = -p; return 0; }");
  requireInvalid("struct S { int x; }; "
                 "int main(void) { struct S s; int x; x = !s; "
                 "return 0; }");
}

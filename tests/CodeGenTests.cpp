#include <catch2/catch_test_macros.hpp>

#include "TestSupport.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string compile_or_fail(const std::string& source, bool optimize = false) {
    TestCompiler compiler;
    std::string ir;
    std::string error;
    REQUIRE(compiler.compile(source, optimize, &ir, &error));
    return ir;
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

size_t count_occurrences(const std::string& text, const std::string& needle) {
    size_t count = 0;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

} // namespace

TEST_CASE("codegen emits a constant return", "[codegen]") {
    std::string ir = compile_or_fail("int main(void) { return 42; }");
    REQUIRE(contains(ir, "define i32 @main()"));
    REQUIRE(contains(ir, "ret i32 42"));
}

TEST_CASE("codegen emits function calls", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int add(int a, int b) { return a + b; }
int main(void) { return add(40, 2); }
)");
    REQUIRE(contains(ir, "define i32 @add(i32"));
    REQUIRE(contains(ir, "call i32 @add(i32 40, i32 2)"));
    REQUIRE(contains(ir, "ret i32 %"));
}

TEST_CASE("codegen emits globals with zero initializers", "[codegen]") {
    std::string ir = compile_or_fail("int g; char c; int main(void) { return g; }");
    REQUIRE(contains(ir, "@g = global i32 0"));
    REQUIRE(contains(ir, "@c = global i8 0"));
}

TEST_CASE("codegen promotes char loads", "[codegen]") {
    std::string ir = compile_or_fail("int main(void) { char c; c = 'A'; return c; }");
    REQUIRE(contains(ir, "alloca i8"));
    REQUIRE(contains(ir, "load i8"));
    REQUIRE(contains(ir, "sext i8"));
}

TEST_CASE("codegen lowers struct member accesses to GEPs", "[codegen]") {
    std::string ir = compile_or_fail(R"(
struct S { int a; char b; };
int main(void) { struct S s; s.a = 1; s.b = 'x'; return s.a; }
)");
    REQUIRE(contains(ir, "alloca { i32, i8 }"));
    REQUIRE(contains(ir, "getelementptr inbounds nuw { i32, i8 }"));
}

TEST_CASE("codegen lays out comma-separated struct members", "[codegen]") {
    const std::string source = R"(
struct S { int a, b; };
struct T { int *p, q; };
int main(void) {
  struct S s;
  struct T t;
  int x;
  s.a = 1;
  s.b = 2;
  t.p = &x;
  t.q = 3;
  *t.p = 4;
  return s.a + s.b + t.q + x;
}
)";
    std::string ir = compile_or_fail(source);
    REQUIRE(contains(ir, "alloca { i32, i32 }"));
    REQUIRE(contains(ir, "alloca { ptr, i32 }"));
    REQUIRE(contains(ir, "getelementptr inbounds nuw { i32, i32 }"));

    std::string optimized = compile_or_fail(source, true);
    REQUIRE(!optimized.empty());
}

TEST_CASE("codegen materializes string literals", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int main(void) { char *s; s = "hi"; return *s; }
)");
    REQUIRE(contains(ir, "private constant [3 x i8]"));
    REQUIRE(contains(ir, "c\"hi\\00\""));
}

TEST_CASE("codegen scales pointer arithmetic", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int main(void) { int x; int *p; p = &x; return *(p + 1); }
)");
    REQUIRE(contains(ir, "getelementptr i32, ptr"));
}

TEST_CASE("codegen lowers goto and labels", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int main(void) { int x; x = 0; goto end; x = 1; end: return x; }
)");
    REQUIRE(contains(ir, "br label %end"));
    REQUIRE(contains(ir, "end:"));
}

TEST_CASE("codegen lowers conditional expressions to PHIs", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int main(int a) { int x; x = a ? 1 : 2; return x; }
)");
    REQUIRE(contains(ir, "cond.then"));
    REQUIRE(contains(ir, "cond.else"));
    REQUIRE(contains(ir, "phi i32"));
}

TEST_CASE("codegen short-circuits logical operators", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int main(int a) { int x; x = a && (a - 1); return x; }
)");
    REQUIRE(contains(ir, "logic.rhs"));
    REQUIRE(contains(ir, "phi i1"));
}

TEST_CASE("codegen emits void returns", "[codegen]") {
    std::string ir = compile_or_fail("void f(void) { return; } int main(void) { f(); return 0; }");
    REQUIRE(contains(ir, "define void @f()"));
    REQUIRE(contains(ir, "ret void"));
}

TEST_CASE("codegen allocates every parameter in the entry block", "[codegen]") {
    std::string ir = compile_or_fail("int f(int a, char b) { return a + b; }");
    REQUIRE(contains(ir, "alloca i32"));
    REQUIRE(contains(ir, "alloca i8"));
    REQUIRE(contains(ir, "store i32 %a1, ptr"));
    REQUIRE(contains(ir, "store i8 %b2, ptr"));
}

TEST_CASE("codegen hoists locals declared in nested blocks", "[codegen]") {
    std::string ir = compile_or_fail(R"(
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

TEST_CASE("codegen coerces conditional arms in their predecessor blocks", "[codegen]") {
    const std::string then_arm = "int f(int x) { char c; c = 1; return x ? c : 2; }";
    std::string ir = compile_or_fail(then_arm);
    size_t then_block = ir.find("cond.then:");
    size_t then_sext = ir.find("sext i8", then_block);
    size_t then_branch = ir.find("br label %cond.end", then_block);
    REQUIRE(then_block != std::string::npos);
    REQUIRE(then_sext != std::string::npos);
    REQUIRE(then_branch != std::string::npos);
    REQUIRE(then_sext < then_branch);

    const std::string else_arm = "int f(int x) { char c; c = 3; return x ? 2 : c; }";
    ir = compile_or_fail(else_arm);
    size_t else_block = ir.find("cond.else:");
    size_t else_sext = ir.find("sext i8", else_block);
    size_t else_branch = ir.find("br label %cond.end", else_block);
    REQUIRE(else_block != std::string::npos);
    REQUIRE(else_sext != std::string::npos);
    REQUIRE(else_branch != std::string::npos);
    REQUIRE(else_sext < else_branch);

    const std::string nested = "int f(int x) { char c; c = 4; "
                               "return x ? (x ? c : 2) : c; }";
    compile_or_fail(nested);
    compile_or_fail(nested, /*optimize=*/true);
}

TEST_CASE("codegen rejects local entities without storage", "[codegen]") {
    TestCompiler compiler;
    std::string error;
    REQUIRE(compiler.analyze("int main(void) { int x; x = 1; return x; }", &error));

    AST& ast = compiler.ast();
    bool removed = false;
    for (DeclId decl_id : ast.translation_unit().decls) {
        auto* function = std::get_if<FunctionDecl>(&ast.decl(decl_id).node);
        if (!function || !function->body.valid())
            continue;
        auto* body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
        REQUIRE(body != nullptr);
        for (auto it = body->items.begin(); it != body->items.end(); ++it) {
            if (std::holds_alternative<DeclStmt>(ast.stmt(*it).node)) {
                body->items.erase(it);
                removed = true;
                break;
            }
        }
        if (removed)
            break;
    }
    REQUIRE(removed);

    std::string ir;
    REQUIRE_FALSE(compiler.generate(&ir, &error));
    REQUIRE(contains(error, "no storage for local entity 'x'"));
    REQUIRE(ir.empty());
}

TEST_CASE("codegen uses assignment expression values directly", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int main(void) {
  int x;
  int y;
  y = (x = 5) + 1;
  return y;
}
)");
    REQUIRE(contains(ir, "store i32 5, ptr"));
    REQUIRE(contains(ir, "store i32 6, ptr"));
    REQUIRE(count_occurrences(ir, "load i32") == 1);
}

TEST_CASE("codegen keeps address-of and dereference distinct", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int main(void) {
  int x;
  int *p;
  p = &x;
  *p = 7;
  return *p;
}
)");
    REQUIRE(contains(ir, "store ptr"));
    REQUIRE(count_occurrences(ir, "load ptr") == 2);
    REQUIRE(contains(ir, "store i32 7, ptr"));
    REQUIRE(count_occurrences(ir, "load i32") == 1);
}

TEST_CASE("codegen lowers dot and arrow member places", "[codegen]") {
    std::string ir = compile_or_fail(R"(
struct S { int a; char b; };
int main(void) {
  struct S s;
  struct S *p;
  p = &s;
  s.a = 1;
  p->b = 'x';
  return s.a + p->b;
}
)");
    REQUIRE(count_occurrences(ir, "getelementptr inbounds nuw { i32, i8 }") >= 3);
    REQUIRE(contains(ir, "load i8"));
    REQUIRE(contains(ir, "sext i8"));
}

TEST_CASE("codegen decays strings and scales pointer arithmetic", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int main(void) {
  char *s;
  s = "abc";
  return *(s + 2);
}
)");
    REQUIRE(contains(ir, "private constant [4 x i8]"));
    REQUIRE(contains(ir, "getelementptr i8, ptr"));
    REQUIRE(contains(ir, "load i8"));
    REQUIRE(contains(ir, "sext i8"));
}

TEST_CASE("codegen distinguishes direct and indirect calls", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int id(int x) { return x; }
int main(void) {
  int (*fp)(int);
  fp = &id;
  return id(1) + (*fp)(2) + fp(3);
}
)");
    REQUIRE(contains(ir, "call i32 @id(i32 1)"));
    REQUIRE(count_occurrences(ir, "call i32 %") == 2);
}

TEST_CASE("codegen discards void call and conditional results", "[codegen]") {
    std::string ir = compile_or_fail(R"(
void set(int v) { }
int main(int x) {
  set(1);
  x ? set(2) : set(3);
  return 0;
}
)");
    REQUIRE(count_occurrences(ir, "call void @set") == 3);
    REQUIRE(contains(ir, "cond.then"));
    REQUIRE(contains(ir, "cond.else"));
    REQUIRE(!contains(ir, "phi void"));
}

TEST_CASE("codegen preserves short-circuit side effects", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int g;
int bump(void) { g = g + 1; return 1; }
int main(void) {
  if (bump() && bump())
    return g;
  return 0;
}
)");
    REQUIRE(contains(ir, "logic.rhs"));
    REQUIRE(count_occurrences(ir, "call i32 @bump()") == 2);
    REQUIRE(contains(ir, "phi i1"));
}

TEST_CASE("codegen lowers nested loops with break, continue and goto", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int main(void) {
  int i;
  int j;
  i = 0;
  j = 0;
  while (i < 2) {
    i = i + 1;
    while (j < 2) {
      j = j + 1;
      if (j == 1)
        continue;
      if (j == 2)
        break;
    }
    goto done;
  }
done:
  return i + j;
}
)");
    REQUIRE(count_occurrences(ir, "\nwhile.cond") == 2);
    REQUIRE(count_occurrences(ir, "\nwhile.body") == 2);
    REQUIRE(count_occurrences(ir, "\nwhile.end") == 2);
    REQUIRE(contains(ir, "br label %done"));
    REQUIRE(contains(ir, "done:"));
}

TEST_CASE("codegen keeps function-scoped state separate", "[codegen]") {
    std::string ir = compile_or_fail(R"(
int f(void) {
  int x;
  x = 1;
  goto done;
  x = 2;
done:
  return x;
}
int g(void) {
  int y;
  y = 3;
  goto done;
  y = 4;
done:
  return y;
}
int main(void) { return f() + g(); }
)");
    REQUIRE(contains(ir, "define i32 @f()"));
    REQUIRE(contains(ir, "define i32 @g()"));
    REQUIRE(count_occurrences(ir, "done:") == 2);
    REQUIRE(count_occurrences(ir, "alloca i32") == 2);
    REQUIRE(contains(ir, "call i32 @f()"));
    REQUIRE(contains(ir, "call i32 @g()"));
}

TEST_CASE("codegen promotes nested mixed char and int conditionals", "[codegen]") {
    std::string ir = compile_or_fail("int f(int x) { char c; c = 5; return x ? (x ? c : 2) : c; }");
    REQUIRE(count_occurrences(ir, "sext i8") == 2);
    REQUIRE(contains(ir, "phi i32"));
}

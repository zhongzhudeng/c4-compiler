#include "TestSupport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <variant>

TEST_CASE("parser accepts the restricted C4 grammar", "[parser]") {
  TestCompiler compiler;
  REQUIRE(compiler.parse("int main(void) { return 0; }"));
  REQUIRE(compiler.parse("int f(int x, int y) { return x + y; }"));
  REQUIRE(compiler.parse("struct S { int x; }; int main(void) { return 0; }"));
  REQUIRE(compiler.parse("int *f(int); int (*g)(int);"));
  REQUIRE(compiler.parse("int main(void) { if (1) return 0; else return 1; }"));
  REQUIRE(compiler.parse("int main(void) { while (1) { break; } return 0; }"));
  REQUIRE(compiler.parse("int main(void) { goto end; end: return 0; }"));
  REQUIRE(compiler.parse("int main(void) { return sizeof(int *); }"));
  REQUIRE(compiler.parse("int main(void) { return sizeof(int (*)(int)); }"));
}

TEST_CASE("parser rejects words outside the restricted grammar", "[parser]") {
  TestCompiler compiler;
  REQUIRE_FALSE(compiler.parse("int main(void) { return 0 }"));
  REQUIRE_FALSE(compiler.parse("int main(void) { a || b = c; }"));
  REQUIRE_FALSE(compiler.parse("int f(;"));
  REQUIRE_FALSE(compiler.parse("int main(void) { int f(void) { return 0; } }"));
  REQUIRE_FALSE(compiler.parse("int main(void) { return 1 ? 2; }"));
  REQUIRE_FALSE(compiler.parse("int main(void) { return a + ; }"));
}

TEST_CASE("parser preserves binary associativity", "[parser]") {
  TestCompiler compiler;
  REQUIRE(compiler.parse("int main(void) { return a * b + d + e; }"));

  const AST &ast = compiler.ast();
  const auto &declarations = ast.translationUnit().decls;
  REQUIRE(declarations.size() == 1);

  auto *function = std::get_if<FunctionDecl>(&ast.decl(declarations[0]).node);
  REQUIRE(function != nullptr);

  auto *body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
  REQUIRE(body != nullptr);
  REQUIRE(body->items.size() == 1);

  auto *returnStmt = std::get_if<ReturnStmt>(&ast.stmt(body->items[0]).node);
  REQUIRE(returnStmt != nullptr);

  auto *outerAdd = std::get_if<BinaryExpr>(&ast.expr(returnStmt->expr).node);
  REQUIRE(outerAdd != nullptr);
  REQUIRE(outerAdd->op == BinaryExpr::Op::Add);

  auto *innerAdd = std::get_if<BinaryExpr>(&ast.expr(outerAdd->lhs).node);
  REQUIRE(innerAdd != nullptr);
  REQUIRE(innerAdd->op == BinaryExpr::Op::Add);

  auto *mul = std::get_if<BinaryExpr>(&ast.expr(innerAdd->lhs).node);
  REQUIRE(mul != nullptr);
  REQUIRE(mul->op == BinaryExpr::Op::Mul);
}

TEST_CASE("parser keeps abstract pointer declarators", "[parser]") {
  TestCompiler compiler;
  REQUIRE(compiler.parse("int main(void) { return sizeof(int *); }"));

  const AST &ast = compiler.ast();
  auto *function = std::get_if<FunctionDecl>(
      &ast.decl(ast.translationUnit().decls[0]).node);
  REQUIRE(function != nullptr);
  auto *body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
  REQUIRE(body != nullptr);
  auto *returnStmt = std::get_if<ReturnStmt>(&ast.stmt(body->items[0]).node);
  REQUIRE(returnStmt != nullptr);
  auto *sizeOf = std::get_if<SizeofExpr>(&ast.expr(returnStmt->expr).node);
  REQUIRE(sizeOf != nullptr);
  REQUIRE(sizeOf->isType);
  REQUIRE(sizeOf->typeDeclarator.valid());
  REQUIRE(std::holds_alternative<PointerDeclarator>(
      ast.declarator(sizeOf->typeDeclarator).node));
}

TEST_CASE("parser classifies function declarators by their name shape",
          "[parser]") {
  TestCompiler compiler;
  REQUIRE(compiler.parse("int *f(int); int (*g)(int);"));

  const AST &ast = compiler.ast();
  const auto &declarations = ast.translationUnit().decls;
  REQUIRE(declarations.size() == 2);

  auto *f = std::get_if<FunctionDecl>(&ast.decl(declarations[0]).node);
  REQUIRE(f != nullptr);
  REQUIRE(declaratorProducesFunction(ast, f->declarator));

  auto *g = std::get_if<VarDecl>(&ast.decl(declarations[1]).node);
  REQUIRE(g != nullptr);
  REQUIRE_FALSE(declaratorProducesFunction(ast, g->declarator));
}

TEST_CASE("parser accepts duplicate definitions for semantic analysis",
          "[parser]") {
  TestCompiler compiler;
  REQUIRE(compiler.parse(
      "int f(void) { return 0; } int f(void) { return 1; }"));
}

TEST_CASE("parser rejects an empty translation unit", "[parser]") {
  TestCompiler compiler;
  std::string error;
  REQUIRE_FALSE(compiler.parse("", &error));
  REQUIRE(error.find("test.c:1:1: error: expected external declaration") !=
          std::string::npos);
  REQUIRE_FALSE(compiler.parse("// a comment is not an external declaration\n"));
  REQUIRE_FALSE(compiler.parse(" \n\t\n"));
}

TEST_CASE("AST rejects malformed node ids", "[ast]") {
  AST ast;
  REQUIRE_THROWS_AS(ast.expr(ExprId{}), std::out_of_range);
  REQUIRE_THROWS_AS(ast.expr(ExprId(0)), std::out_of_range);
  REQUIRE_THROWS_AS(ast.decl(DeclId(7)), std::out_of_range);
  REQUIRE_THROWS_AS(ast.typeSpec(TypeSpecId(1)), std::out_of_range);
}

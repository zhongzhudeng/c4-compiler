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

TEST_CASE("parser expands comma-separated struct members", "[parser]") {
    TestCompiler compiler;
    REQUIRE(compiler.parse("struct S { int a, *p, b; };"));

    const AST& ast = compiler.ast();
    const auto& declarations = ast.translation_unit().decls;
    REQUIRE(declarations.size() == 1);

    auto* tag = std::get_if<TagDecl>(&ast.decl(declarations[0]).node);
    REQUIRE(tag != nullptr);
    const TypeSpec& spec = ast.type_spec(tag->spec);
    auto* structure = std::get_if<TypeSpec::StructDefinition>(&spec.node);
    REQUIRE(structure != nullptr);
    REQUIRE(structure->fields.size() == 3);

    const char* names[] = {"a", "p", "b"};
    TypeSpecId member_spec = std::get<FieldDecl>(ast.decl(structure->fields[0]).node).spec;
    for (size_t i = 0; i < structure->fields.size(); ++i) {
        auto* field = std::get_if<FieldDecl>(&ast.decl(structure->fields[i]).node);
        REQUIRE(field != nullptr);
        REQUIRE(field->spec == member_spec);
        REQUIRE(*declarator_name(ast, field->declarator) == names[i]);
    }
    REQUIRE(std::holds_alternative<PointerDeclarator>(
        ast.declarator(std::get<FieldDecl>(ast.decl(structure->fields[1]).node).declarator).node));
}

TEST_CASE("parser rejects words outside the restricted grammar", "[parser]") {
    TestCompiler compiler;
    REQUIRE_FALSE(compiler.parse("int main(void) { return 0 }"));
    REQUIRE_FALSE(compiler.parse("int a, b;"));
    REQUIRE_FALSE(compiler.parse("int main(void) { a || b = c; }"));
    REQUIRE_FALSE(compiler.parse("int f(;"));
    REQUIRE_FALSE(compiler.parse("int main(void) { int f(void) { return 0; } }"));
    REQUIRE_FALSE(compiler.parse("int main(void) { return 1 ? 2; }"));
    REQUIRE_FALSE(compiler.parse("int main(void) { return a + ; }"));
    REQUIRE_FALSE(compiler.parse("int main(void) { int a; (a = 1) = 2; }"));
}

TEST_CASE("parser preserves binary associativity", "[parser]") {
    TestCompiler compiler;
    REQUIRE(compiler.parse("int main(void) { return a * b + d + e; }"));

    const AST& ast = compiler.ast();
    const auto& declarations = ast.translation_unit().decls;
    REQUIRE(declarations.size() == 1);

    auto* function = std::get_if<FunctionDecl>(&ast.decl(declarations[0]).node);
    REQUIRE(function != nullptr);

    auto* body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
    REQUIRE(body != nullptr);
    REQUIRE(body->items.size() == 1);

    auto* return_stmt = std::get_if<ReturnStmt>(&ast.stmt(body->items[0]).node);
    REQUIRE(return_stmt != nullptr);

    auto* outer_add = std::get_if<BinaryExpr>(&ast.expr(return_stmt->expr).node);
    REQUIRE(outer_add != nullptr);
    REQUIRE(outer_add->op == BinaryExpr::Op::Add);

    auto* inner_add = std::get_if<BinaryExpr>(&ast.expr(outer_add->lhs).node);
    REQUIRE(inner_add != nullptr);
    REQUIRE(inner_add->op == BinaryExpr::Op::Add);

    auto* mul = std::get_if<BinaryExpr>(&ast.expr(inner_add->lhs).node);
    REQUIRE(mul != nullptr);
    REQUIRE(mul->op == BinaryExpr::Op::Mul);
}

TEST_CASE("parser keeps abstract pointer declarators", "[parser]") {
    TestCompiler compiler;
    REQUIRE(compiler.parse("int main(void) { return sizeof(int *); }"));

    const AST& ast = compiler.ast();
    auto* function = std::get_if<FunctionDecl>(&ast.decl(ast.translation_unit().decls[0]).node);
    REQUIRE(function != nullptr);
    auto* body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
    REQUIRE(body != nullptr);
    auto* return_stmt = std::get_if<ReturnStmt>(&ast.stmt(body->items[0]).node);
    REQUIRE(return_stmt != nullptr);
    auto* size_of = std::get_if<SizeofExpr>(&ast.expr(return_stmt->expr).node);
    REQUIRE(size_of != nullptr);
    auto* type_name = std::get_if<SizeofExpr::TypeName>(&size_of->target);
    REQUIRE(type_name != nullptr);
    REQUIRE(type_name->declarator.valid());
    REQUIRE(std::holds_alternative<PointerDeclarator>(ast.declarator(type_name->declarator).node));
}

TEST_CASE("parser classifies function declarators by their name shape", "[parser]") {
    TestCompiler compiler;
    REQUIRE(compiler.parse("int *f(int); int (*g)(int);"));

    const AST& ast = compiler.ast();
    const auto& declarations = ast.translation_unit().decls;
    REQUIRE(declarations.size() == 2);

    auto* f = std::get_if<FunctionDecl>(&ast.decl(declarations[0]).node);
    REQUIRE(f != nullptr);
    REQUIRE(declarator_produces_function(ast, f->declarator));

    auto* g = std::get_if<VarDecl>(&ast.decl(declarations[1]).node);
    REQUIRE(g != nullptr);
    REQUIRE_FALSE(declarator_produces_function(ast, g->declarator));
}

TEST_CASE("parser accepts duplicate definitions for semantic analysis", "[parser]") {
    TestCompiler compiler;
    REQUIRE(compiler.parse("int f(void) { return 0; } int f(void) { return 1; }"));
}

TEST_CASE("parser rejects an empty translation unit", "[parser]") {
    TestCompiler compiler;
    std::string error;
    REQUIRE_FALSE(compiler.parse("", &error));
    REQUIRE(error.find("test.c:1:1: error: expected external declaration") != std::string::npos);
    REQUIRE_FALSE(compiler.parse("// a comment is not an external declaration\n"));
    REQUIRE_FALSE(compiler.parse(" \n\t\n"));
}

TEST_CASE("AST rejects malformed node ids", "[ast]") {
    AST ast;
    REQUIRE_THROWS_AS(ast.expr(ExprId{}), std::out_of_range);
    REQUIRE_THROWS_AS(ast.expr(ExprId(0)), std::out_of_range);
    REQUIRE_THROWS_AS(ast.decl(DeclId(7)), std::out_of_range);
    REQUIRE_THROWS_AS(ast.type_spec(TypeSpecId(1)), std::out_of_range);
}

TEST_CASE("parser preserves parenthesized expressions", "[parser]") {
    TestCompiler compiler;
    REQUIRE(compiler.parse("int main(void) { int x; (x) = 1; return (1 + 2); }"));

    const AST& ast = compiler.ast();
    const auto* function =
        std::get_if<FunctionDecl>(&ast.decl(ast.translation_unit().decls[0]).node);
    REQUIRE(function != nullptr);
    const auto* body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
    REQUIRE(body != nullptr);
    REQUIRE(body->items.size() == 3);

    const auto* assignment_statement = std::get_if<ExprStmt>(&ast.stmt(body->items[1]).node);
    REQUIRE(assignment_statement != nullptr);
    const auto* assignment = std::get_if<AssignExpr>(&ast.expr(assignment_statement->expr).node);
    REQUIRE(assignment != nullptr);
    const auto* target = std::get_if<ParenExpr>(&ast.expr(assignment->lhs).node);
    REQUIRE(target != nullptr);
    CHECK(std::holds_alternative<IdentifierExpr>(ast.expr(target->inner).node));

    const auto* return_statement = std::get_if<ReturnStmt>(&ast.stmt(body->items[2]).node);
    REQUIRE(return_statement != nullptr);
    const auto* result = std::get_if<ParenExpr>(&ast.expr(return_statement->expr).node);
    REQUIRE(result != nullptr);
    CHECK(std::holds_alternative<BinaryExpr>(ast.expr(result->inner).node));
}

TEST_CASE("parser distinguishes function parameter clauses", "[parser]") {
    TestCompiler compiler;
    REQUIRE(compiler.parse("int a(); int b(void); int c(int, char *);"));

    const AST& ast = compiler.ast();
    const auto& declarations = ast.translation_unit().decls;
    REQUIRE(declarations.size() == 3);

    auto parameters = [&](size_t index) -> const FunctionDeclarator::Parameters& {
        const auto* function = std::get_if<FunctionDecl>(&ast.decl(declarations[index]).node);
        REQUIRE(function != nullptr);
        const auto* declarator =
            std::get_if<FunctionDeclarator>(&ast.declarator(function->declarator).node);
        REQUIRE(declarator != nullptr);
        return declarator->parameters;
    };

    CHECK(std::holds_alternative<FunctionDeclarator::NoParameters>(parameters(0)));
    CHECK(std::holds_alternative<FunctionDeclarator::VoidParameters>(parameters(1)));
    REQUIRE(std::holds_alternative<FunctionDeclarator::ParameterList>(parameters(2)));
    CHECK(std::get<FunctionDeclarator::ParameterList>(parameters(2)).size() == 2);
}

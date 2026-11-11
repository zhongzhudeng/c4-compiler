#include "TestSupport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

void require_valid(const std::string& source) {
    TestCompiler compiler;
    std::string error;
    INFO(source);
    REQUIRE(compiler.analyze(source, &error));
}

void require_invalid(const std::string& source) {
    TestCompiler compiler;
    std::string error;
    INFO(source);
    REQUIRE_FALSE(compiler.analyze(source, &error));
    REQUIRE(error.find("test.c:") != std::string::npos);
    REQUIRE(error.find(": error: ") != std::string::npos);
}

} // namespace

TEST_CASE("sema resolves names, types, pointers, and calls", "[sema]") {
    require_valid("int main(void) { return 0; }");
    require_valid("int f(int x, int y) { return x + y; } "
                  "int main(void) { return f(1, 2); }");
    require_valid("int main(void) { int *p; int i; p = &i; *p = 1; "
                  "return i; }");
    require_valid("int main(void) { int *p; p = 0; return p == 0; }");
    require_valid("int main(void) { char *p; p = \"abc\"; return p[0]; }");
    require_valid("int main(void) { int *p; void *v; p = v; v = p; "
                  "return p == v; }");
    require_valid("int main(void) { int *p; int *q; return p < q; }");
    require_valid("int main(void) { int *p; int *q; return p - q; }");
    require_valid("int f(int x) { return x; } "
                  "int main(void) { int (*p)(int); p = f; return p(1); }");
    require_valid("int f(int x) { return x; } "
                  "int main(void) { int (*p)(int); p = &f; return (*p)(1); }");
    require_valid("int main(void) { int (*p)(void); p = 0; return p == 0; }");
    require_valid("int main(void) { int a; int b; int c; a = b = c = 1; "
                  "return a; }");
    require_valid("int main(void) { int a; a = 1 ? 2 : 3; return a; }");
    require_valid("void f(void) {} int main(void) { 1 ? f() : f(); "
                  "return 0; }");
    require_valid("int main(void) { int x; x = sizeof(int); "
                  "x = sizeof(int *); x = sizeof \"abc\"; return x; }");
}

TEST_CASE("sema handles structs and tags", "[sema]") {
    require_valid("struct S { int x; int y; }; "
                  "int main(void) { struct S s; struct S *p; p = &s; "
                  "p->x = 1; (*p).y = 2; return s.x + s.y; }");
    require_valid("struct S { struct S *next; int x; }; "
                  "int main(void) { struct S a; struct S b; a.next = &b; "
                  "a.next->x = 3; return a.next->x; }");
    require_valid("struct S; struct S *p; struct S { int x; }; "
                  "int main(void) { struct S s; p = &s; p->x = 4; "
                  "return p->x; }");
    require_valid("int main(void) { struct S { int x; } s; s.x = 1; "
                  "return s.x; }");
    require_valid("int main(void) { struct { int x; } s; s.x = 1; "
                  "return s.x; }");
    require_valid("struct S { int x; }; "
                  "void f(struct S *p) { p->x = 1; } "
                  "int main(void) { struct S s; f(&s); return s.x; }");
    require_valid("struct S { int a, b; }; "
                  "int main(void) { struct S s; s.a = 1; s.b = 2; "
                  "return s.a + s.b; }");
    require_valid("struct S { int *p, q; }; "
                  "int main(void) { struct S s; int x; s.p = &x; s.q = 2; "
                  "return s.q; }");
    require_valid("int f(struct S *p) { struct S *q; return 0; }");
    require_valid("struct S; int f(struct S *p); struct S { int x; }; "
                  "int f(struct S *p) { return 0; }");
    require_valid("struct S { struct T { int x; } a, b; }; "
                  "int main(void) { struct S s; s.a.x = 1; s.b.x = 2; "
                  "return s.a.x + s.b.x; }");
    require_valid("int (*f(struct S *p))(int) { struct S *q; q = p; "
                  "return 0; }");
    require_valid("struct S; int f(int (*g)(struct S *), struct S *p); "
                  "struct S { int x; }; "
                  "int f(int (*g)(struct S *), struct S *p) { return 0; }");
}

TEST_CASE("sema handles labels, loops, and returns", "[sema]") {
    require_valid("int main(void) { goto end; end: return 0; }");
    require_valid("int main(void) { goto end; { end: ; } return 0; }");
    require_valid("int main(void) { a: b: return 0; }");
    require_valid("int main(void) { while (1) { if (1) break; "
                  "else continue; } return 0; }");
    require_valid("void f(void) { return; } int main(void) { f(); "
                  "return 0; }");
    require_valid("int main(void) { int x; { int x; x = 1; } return x; }");
    require_valid("int f(int x) { { int x; x = 1; } return x; } "
                  "int main(void) { return f(0); }");
}

TEST_CASE("sema rejects invalid programs", "[sema]") {
    require_invalid("int main(void) { return x; }");
    require_invalid("int main(void) { int x; int x; return 0; }");
    require_invalid("int f(int); int f(char); int main(void) { return 0; }");
    require_invalid("int f(void) { return 0; } int f(void) { return 1; } "
                    "int main(void) { return 0; }");
    require_invalid("int main(void) { int a; int b; a + b = 1; return 0; }");
    require_invalid("int f(void) { return 1; } "
                    "int main(void) { f() = 1; return 0; }");
    require_invalid("int main(void) { \"abc\" = \"def\"; return 0; }");
    require_invalid("int f(void) { return 1; } "
                    "int main(void) { f = 0; return 0; }");
    require_invalid("int main(void) { int *p; char *q; p = q; return 0; }");
    require_invalid("int main(void) { int *p; p = 1; return 0; }");
    require_invalid("int main(void) { void *p; p = p + 1; return 0; }");
    require_invalid("int main(void) { int (*p)(void); p = p + 1; return 0; }");
    require_invalid("int main(void) { int x; x = *x; return 0; }");
    require_invalid("int main(void) { void *p; *p; return 0; }");
    require_invalid("int main(void) { int x; x(); return 0; }");
    require_invalid("int f(int x) { return x; } "
                    "int main(void) { return f(); }");
    require_invalid("int f(int *p) { return 0; } "
                    "int main(void) { int x; return f(x); }");
    require_invalid("int main(void) { int x; return x.y; }");
    require_invalid("struct S { int x; }; "
                    "int main(void) { struct S s; return s.y; }");
    require_invalid("struct S { int x; }; "
                    "int main(void) { struct S s; return s->x; }");
    require_invalid("struct S { int x; }; "
                    "int main(void) { struct S *p; return p.x; }");
    require_invalid("int main(void) { return sizeof(void); }");
    require_invalid("int f(void) { return 0; } "
                    "int main(void) { return sizeof(f); }");
    require_invalid("struct S; int main(void) { return sizeof(struct S); }");
    require_invalid("struct S { int x; }; "
                    "int main(void) { struct S s; if (s) return 1; "
                    "return 0; }");
    require_invalid("void f(void) {} int main(void) { if (f()) return 1; "
                    "return 0; }");
    require_invalid("int main(void) { break; return 0; }");
    require_invalid("int main(void) { continue; return 0; }");
    require_invalid("int main(void) { goto nowhere; return 0; }");
    require_invalid("int main(void) { a: a: return 0; }");
    require_invalid("void f(void) { return 1; } int main(void) { return 0; }");
    require_invalid("int f(void) { return; } int main(void) { return 0; }");
    require_invalid("int f(void) { return \"x\"; } "
                    "int main(void) { return 0; }");
    require_invalid("int main(void) { void x; return 0; }");
    require_invalid("int f(void)(void); int main(void) { return 0; }");
    require_invalid("struct S { int x; }; struct S f(void); "
                    "int main(void) { return 0; }");
    require_invalid("struct S { int x; }; void f(struct S s); "
                    "int main(void) { return 0; }");
    require_invalid("struct S; int main(void) { struct S s; return 0; }");
    require_invalid("struct S { struct S s; }; int main(void) { return 0; }");
    require_invalid("struct S { int x; int x; }; int main(void) { return 0; }");
    require_invalid("struct S { int a, a; }; int main(void) { return 0; }");
    require_invalid("struct { int x; }; int main(void) { return 0; }");
    require_invalid("int f(struct S *p); struct S { int x; }; "
                    "int f(struct S *p) { return 0; }");
    require_invalid("int f(struct S *p); int f(struct S *p);");
    require_invalid("int f(int (*g)(struct S *), struct S *p); "
                    "struct S { int x; }; "
                    "int f(int (*g)(struct S *), struct S *p) { return 0; }");
    // A tag in the return type's parameter list has prototype scope and must
    // not be visible in the function body.
    require_invalid("int (*f(struct S *p))(struct T { int x; } *) { "
                    "struct T *r; r->x = 1; return 0; }");
    // A tag declared in a nested parameter declarator must not leak into the
    // enclosing function's body either.
    require_invalid("int f(int g(struct T { int x; } *p)) { "
                    "struct T *r; r->x = 1; return 0; }");
    require_invalid("struct S { int x; }; struct S { int y; }; "
                    "int main(void) { return 0; }");
    require_invalid("struct S { }; int main(void) { return 0; }");
    require_invalid("void f(void x); int main(void) { return 0; }");
    require_invalid("int f(int x, int x); int main(void) { return 0; }");
    require_invalid("int f(int) { return 0; } int main(void) { return 0; }");
    require_invalid("int main(void) { int f(void) { return 0; } return 0; }");
    require_invalid("int x; char x; int main(void) { return 0; }");
    require_invalid("int f(void); int f; int main(void) { return 0; }");
    require_invalid("int x; int x(void); int main(void) { return 0; }");
    require_invalid("int main(void) { return 2147483648; }");
    require_invalid("int main(void) { int *p; char *q; int x; "
                    "p = x ? p : q; return 0; }");
    require_invalid("struct S { int x; }; "
                    "int main(void) { struct S a; struct S b; a = b; "
                    "return 0; }");
    require_invalid("int main(void) { int *p; int *q; p = p + q; return 0; }");
    require_invalid("int main(void) { void *p; void *q; int x; x = p - q; "
                    "return 0; }");
    require_invalid("int main(void) { void *p; void *q; int x; x = p < q; "
                    "return 0; }");
    require_invalid("int f(void) { return 0; } "
                    "int main(void) { void *p; int x; x = f == p; "
                    "return 0; }");
    require_invalid("struct S; int main(void) { struct S *p; struct S *q; "
                    "int x; x = p - q; return 0; }");
    require_invalid("struct S; int main(void) { struct S *p; p = p + 1; "
                    "return 0; }");
    require_invalid("struct S; int main(void) { struct S *p; return p->x; }");
    require_invalid("int main(void) { int *p; int x; x = p < 0; return 0; }");
    require_invalid("int main(void) { int *p; char *q; int x; x = p == q; "
                    "return 0; }");
    require_invalid("void f(void) {} int main(void) { f() ? 1 : 2; "
                    "return 0; }");
    require_invalid("int main(void) { int x; x = &(x + 1); return 0; }");
    require_invalid("int main(void) { int *p; p = -p; return 0; }");
    require_invalid("struct S { int x; }; "
                    "int main(void) { struct S s; int x; x = !s; "
                    "return 0; }");
}

TEST_CASE("sema scopes struct tags and completes tentative definitions", "[sema]") {
    // A block-scope "struct S;" declares a new tag instead of reusing the
    // outer definition, so the two pointer types are incompatible.
    require_invalid(R"(
struct S { int a; };
int main(void) {
  struct S *p;
  {
    struct S;
    struct S *q;
    p = q;
  }
  return 0;
}
)");
    require_invalid(R"(
struct S { int a; };
int main(void) {
  struct S *p;
  {
    struct S;
    struct S { int b; };
    struct S *q;
    p = q;
  }
  return 0;
}
)");
    require_valid(R"(
struct S { int a; };
int main(void) {
  struct S *p;
  {
    struct S;
    struct S *q;
    q = q;
  }
  return 0;
}
)");

    // A file-scope tentative definition may start incomplete and be completed
    // later in the translation unit; it is invalid only if never completed.
    require_valid("struct S; struct S g; struct S { int x; }; "
                  "int main(void) { return sizeof(g); }");
    require_invalid("struct S; struct S g; int main(void) { return 0; }");
    // The type is still incomplete at this use, even though it is completed
    // later in the translation unit.
    require_invalid("struct S; struct S g; "
                    "int main(void) { return sizeof(g); } "
                    "struct S { int x; };");
    require_invalid("struct S; int main(void) { struct S s; return 0; }");
}

TEST_CASE("sema lets block-scope functions hide outer block objects", "[sema]") {
    require_valid("int f(void) { return 42; } "
                  "int main(void) { int f; { int f(void); return f(); } }");
    require_valid("int main(void) { int f; { int f(void); return 0; } }");
    // A file-scope object has external linkage, so it conflicts with the
    // block-scope function declaration.
    require_invalid("int f; int main(void) { { int f(void); return 0; } }");
    require_invalid("int f(void) { return 0; } "
                    "int main(void) { int f; { int f(int); return 0; } }");
}

namespace {

DeclId top_level_decl(const TestCompiler& compiler, size_t index) {
    return compiler.ast().translation_unit().decls.at(index);
}

Symbol decl_name(const TestCompiler& compiler, DeclId id) {
    const AST& ast = compiler.ast();
    const Decl& decl = ast.decl(id);
    if (auto* var = std::get_if<VarDecl>(&decl.node))
        return declarator_name(ast, var->declarator);
    if (auto* function = std::get_if<FunctionDecl>(&decl.node))
        return declarator_name(ast, function->declarator);
    if (auto* param = std::get_if<ParamDecl>(&decl.node))
        return declarator_name(ast, param->declarator);
    if (auto* field = std::get_if<FieldDecl>(&decl.node))
        return declarator_name(ast, field->declarator);
    return Symbol();
}

void collect_declarator_params(const AST& ast, DeclaratorId id, std::vector<DeclId>& out) {
    if (!id.valid())
        return;
    const Declarator& declarator = ast.declarator(id);
    if (auto* pointer = std::get_if<PointerDeclarator>(&declarator.node)) {
        collect_declarator_params(ast, pointer->inner, out);
        return;
    }
    if (auto* function = std::get_if<FunctionDeclarator>(&declarator.node)) {
        if (auto* params = std::get_if<FunctionDeclarator::ParameterList>(&function->parameters))
            out.insert(out.end(), params->begin(), params->end());
        collect_declarator_params(ast, function->inner, out);
    }
}

std::vector<DeclId> declarator_params(const AST& ast, DeclaratorId id) {
    std::vector<DeclId> result;
    collect_declarator_params(ast, id, result);
    return result;
}

void collect_stmt_decls(const AST& ast, StmtId id, std::vector<DeclId>& out) {
    if (!id.valid())
        return;
    const Stmt& stmt = ast.stmt(id);
    if (auto* compound = std::get_if<CompoundStmt>(&stmt.node)) {
        for (StmtId item : compound->items)
            collect_stmt_decls(ast, item, out);
        return;
    }
    if (auto* decl_stmt = std::get_if<DeclStmt>(&stmt.node)) {
        out.push_back(decl_stmt->decl);
        return;
    }
    if (auto* if_stmt = std::get_if<IfStmt>(&stmt.node)) {
        collect_stmt_decls(ast, if_stmt->then_stmt, out);
        collect_stmt_decls(ast, if_stmt->else_stmt, out);
        return;
    }
    if (auto* while_stmt = std::get_if<WhileStmt>(&stmt.node)) {
        collect_stmt_decls(ast, while_stmt->body, out);
        return;
    }
    if (auto* label_stmt = std::get_if<LabelStmt>(&stmt.node))
        collect_stmt_decls(ast, label_stmt->stmt, out);
}

std::vector<DeclId> local_decls(const TestCompiler& compiler) {
    std::vector<DeclId> result;
    const AST& ast = compiler.ast();
    for (DeclId id : ast.translation_unit().decls) {
        const Decl& decl = ast.decl(id);
        if (auto* function = std::get_if<FunctionDecl>(&decl.node))
            if (function->body.valid())
                collect_stmt_decls(ast, function->body, result);
    }
    return result;
}

ScopeId only_function_scope(const SemanticModel& model) {
    ScopeId result;
    for (size_t i = 0; i < model.scope_count(); ++i) {
        ScopeId id{static_cast<uint32_t>(i)};
        if (model.scope(id).kind != ScopeKind::Function)
            continue;
        REQUIRE_FALSE(result.valid());
        result = id;
    }
    REQUIRE(result.valid());
    return result;
}

} // namespace

TEST_CASE("sema gives compatible declarations one entity", "[sema][architecture]") {
    const std::string source = R"(
int f(int);
int f(int x) { return x; }
int f(int);
)";
    TestCompiler compiler;
    std::string error;
    INFO(source);
    INFO(error);
    REQUIRE(compiler.analyze(source, &error));

    const AST& ast = compiler.ast();
    const SemanticModel& model = compiler.model();
    REQUIRE(ast.translation_unit().decls.size() == 3);
    DeclId first = top_level_decl(compiler, 0);
    DeclId definition = top_level_decl(compiler, 1);
    DeclId last = top_level_decl(compiler, 2);

    EntityId entity = model.decl(first).entity;
    REQUIRE(entity.valid());
    CHECK(model.decl(definition).entity == entity);
    CHECK(model.decl(last).entity == entity);
    CHECK(model.decl(first).scope == model.file_scope());
    CHECK(model.decl(definition).scope == model.file_scope());
    CHECK(model.decl(last).scope == model.file_scope());

    const Entity& function = model.entity(entity);
    CHECK(function.kind == EntityKind::Function);
    CHECK(function.linkage == Linkage::External);
    CHECK(function.definition == DefinitionState::Defined);
    CHECK(function.declarations.size() == 3);

    Symbol name = decl_name(compiler, first);
    REQUIRE(name != nullptr);
    CHECK(model.lookup(model.file_scope(), name) == entity);

    REQUIRE(function.parameters.size() == 1);
    DeclId parameter = function.parameters[0];
    EntityId parameter_entity = model.decl(parameter).entity;
    REQUIRE(parameter_entity.valid());
    CHECK(model.entity(parameter_entity).kind == EntityKind::Object);
    CHECK(model.entity(parameter_entity).linkage == Linkage::None);
    ScopeId function_scope = model.decl(parameter).scope;
    CHECK(model.scope(function_scope).kind == ScopeKind::Function);
    CHECK(model.scope(function_scope).parent == model.file_scope());
    Symbol parameter_name = decl_name(compiler, parameter);
    REQUIRE(parameter_name != nullptr);
    CHECK(model.lookup(function_scope, parameter_name) == parameter_entity);
}

TEST_CASE("sema binds block-scope functions to the external entity", "[sema][architecture]") {
    const std::string source = R"(
int main(void) {
  int f;
  { int f(void); return f(); }
  { int f(void); }
  return f;
}
)";
    TestCompiler compiler;
    std::string error;
    INFO(source);
    INFO(error);
    REQUIRE(compiler.analyze(source, &error));

    const SemanticModel& model = compiler.model();
    std::vector<DeclId> locals = local_decls(compiler);
    REQUIRE(locals.size() == 3);

    EntityId object = model.decl(locals[0]).entity;
    EntityId function = model.decl(locals[1]).entity;
    REQUIRE(object.valid());
    REQUIRE(function.valid());
    CHECK(object != function);
    CHECK(model.entity(object).kind == EntityKind::Object);
    CHECK(model.entity(object).linkage == Linkage::None);
    CHECK(model.entity(function).kind == EntityKind::Function);
    CHECK(model.entity(function).linkage == Linkage::External);
    CHECK(model.decl(locals[2]).entity == function);
    CHECK(model.entity(function).declarations.size() == 2);

    ScopeId object_scope = model.decl(locals[0]).scope;
    ScopeId first_block = model.decl(locals[1]).scope;
    ScopeId second_block = model.decl(locals[2]).scope;
    CHECK(model.scope(object_scope).kind == ScopeKind::Function);
    CHECK(model.scope(first_block).kind == ScopeKind::Block);
    CHECK(model.scope(second_block).kind == ScopeKind::Block);
    CHECK(model.scope(first_block).parent == object_scope);
    CHECK(model.scope(second_block).parent == object_scope);
    CHECK(first_block != second_block);

    Symbol name = decl_name(compiler, locals[0]);
    REQUIRE(name != nullptr);
    CHECK(model.binding(object_scope, name) == object);
    CHECK(model.binding(first_block, name) == function);
    CHECK(model.binding(second_block, name) == function);
    CHECK(model.lookup(first_block, name) == function);
    CHECK(model.lookup(object_scope, name) == object);
}

TEST_CASE("sema keeps prototype tags in function prototype scope", "[sema][architecture]") {
    const std::string source = R"(
int f(struct P { int x; } *p);
int main(void) { struct P *q; return 0; }
)";
    TestCompiler compiler;
    std::string error;
    INFO(source);
    INFO(error);
    REQUIRE(compiler.analyze(source, &error));

    const AST& ast = compiler.ast();
    const SemanticModel& model = compiler.model();
    REQUIRE(ast.translation_unit().decls.size() == 2);
    auto* function = std::get_if<FunctionDecl>(&ast.decl(top_level_decl(compiler, 0)).node);
    REQUIRE(function != nullptr);
    std::vector<DeclId> params = declarator_params(ast, function->declarator);
    REQUIRE(params.size() == 1);
    DeclId parameter = params[0];

    ScopeId prototype = model.decl(parameter).scope;
    CHECK(model.scope(prototype).kind == ScopeKind::FunctionPrototype);
    auto* param_decl = std::get_if<ParamDecl>(&ast.decl(parameter).node);
    REQUIRE(param_decl != nullptr);
    const TypeSpec& spec = ast.type_spec(param_decl->spec);
    auto* structure = std::get_if<TypeSpec::StructDefinition>(&spec.node);
    REQUIRE(structure != nullptr);
    REQUIRE(structure->tag != nullptr);
    StructDecl* prototype_tag = model.type_spec(param_decl->spec).record;
    REQUIRE(prototype_tag != nullptr);
    CHECK(model.binding_tag(prototype, structure->tag) == prototype_tag);
    CHECK(model.binding_tag(model.file_scope(), structure->tag) == nullptr);
    CHECK(model.lookup_tag(model.file_scope(), structure->tag) == nullptr);

    std::vector<DeclId> locals = local_decls(compiler);
    REQUIRE(locals.size() == 1);
    auto* q = std::get_if<VarDecl>(&ast.decl(locals[0]).node);
    REQUIRE(q != nullptr);
    StructDecl* main_tag = model.type_spec(q->spec).record;
    REQUIRE(main_tag != nullptr);
    CHECK(main_tag != prototype_tag);
    ScopeId main_scope = model.decl(locals[0]).scope;
    CHECK(model.scope(main_scope).kind == ScopeKind::Function);
    CHECK(model.binding_tag(main_scope, structure->tag) == main_tag);
    CHECK(model.lookup_tag(main_scope, structure->tag) == main_tag);
}

TEST_CASE("sema keeps definition parameter tags in the function scope", "[sema][architecture]") {
    const std::string source = R"(
int f(struct S { int x; } *p) {
  struct S *q;
  q = p;
  return q->x;
}
)";
    TestCompiler compiler;
    std::string error;
    INFO(source);
    INFO(error);
    REQUIRE(compiler.analyze(source, &error));

    const AST& ast = compiler.ast();
    const SemanticModel& model = compiler.model();
    REQUIRE(ast.translation_unit().decls.size() == 1);
    DeclId definition = top_level_decl(compiler, 0);
    auto* function = std::get_if<FunctionDecl>(&ast.decl(definition).node);
    REQUIRE(function != nullptr);
    std::vector<DeclId> params = declarator_params(ast, function->declarator);
    REQUIRE(params.size() == 1);
    DeclId parameter = params[0];

    ScopeId function_scope = model.decl(parameter).scope;
    CHECK(model.scope(function_scope).kind == ScopeKind::Function);
    CHECK(model.scope(function_scope).parent == model.file_scope());
    auto* param_decl = std::get_if<ParamDecl>(&ast.decl(parameter).node);
    REQUIRE(param_decl != nullptr);
    const TypeSpec& spec = ast.type_spec(param_decl->spec);
    auto* structure = std::get_if<TypeSpec::StructDefinition>(&spec.node);
    REQUIRE(structure != nullptr);
    REQUIRE(structure->tag != nullptr);
    StructDecl* tag = model.type_spec(param_decl->spec).record;
    REQUIRE(tag != nullptr);
    CHECK(tag->complete);
    CHECK(model.binding_tag(function_scope, structure->tag) == tag);
    CHECK(model.binding_tag(model.file_scope(), structure->tag) == nullptr);

    EntityId entity_id = model.decl(definition).entity;
    REQUIRE(entity_id.valid());
    const Entity& entity = model.entity(entity_id);
    CHECK(entity.definition == DefinitionState::Defined);
    REQUIRE(entity.parameters.size() == 1);
    CHECK(entity.parameters[0] == parameter);
    CHECK(model.entity(model.decl(parameter).entity).linkage == Linkage::None);

    std::vector<DeclId> locals = local_decls(compiler);
    REQUIRE(locals.size() == 1);
    auto* q = std::get_if<VarDecl>(&ast.decl(locals[0]).node);
    REQUIRE(q != nullptr);
    CHECK(model.decl(locals[0]).scope == function_scope);
    CHECK(model.type_spec(q->spec).record == tag);
    Symbol parameter_name = decl_name(compiler, parameter);
    REQUIRE(parameter_name != nullptr);
    CHECK(model.lookup(function_scope, parameter_name) == model.decl(parameter).entity);
}

TEST_CASE("sema adopts only the defining function declarator's scope", "[sema][architecture]") {
    const std::string source = R"(
int (*f(struct S *p))(int) {
  struct S *q;
  q = p;
  return 0;
}
)";
    TestCompiler compiler;
    std::string error;
    INFO(source);
    INFO(error);
    REQUIRE(compiler.analyze(source, &error));

    const AST& ast = compiler.ast();
    const SemanticModel& model = compiler.model();
    REQUIRE(ast.translation_unit().decls.size() == 1);
    DeclId definition = top_level_decl(compiler, 0);
    EntityId entity_id = model.decl(definition).entity;
    REQUIRE(entity_id.valid());
    const Entity& entity = model.entity(entity_id);
    CHECK(entity.kind == EntityKind::Function);
    REQUIRE(entity.parameters.size() == 1);
    DeclId parameter = entity.parameters[0];

    ScopeId function_scope = model.decl(parameter).scope;
    CHECK(model.scope(function_scope).kind == ScopeKind::Function);
    CHECK(model.scope(function_scope).parent == model.file_scope());
    auto* param_decl = std::get_if<ParamDecl>(&ast.decl(parameter).node);
    REQUIRE(param_decl != nullptr);
    const TypeSpec& spec = ast.type_spec(param_decl->spec);
    auto* structure = std::get_if<TypeSpec::StructReference>(&spec.node);
    REQUIRE(structure != nullptr);
    REQUIRE(structure->tag != nullptr);
    StructDecl* tag = model.type_spec(param_decl->spec).record;
    REQUIRE(tag != nullptr);
    CHECK(model.binding_tag(function_scope, structure->tag) == tag);
    CHECK(model.binding_tag(model.file_scope(), structure->tag) == nullptr);

    size_t prototypes = 0;
    for (size_t i = 0; i < model.scope_count(); ++i) {
        ScopeId id{static_cast<uint32_t>(i)};
        if (model.scope(id).kind != ScopeKind::FunctionPrototype)
            continue;
        ++prototypes;
        CHECK(model.scope(id).tags.empty());
    }
    CHECK(prototypes == 1);

    std::vector<DeclId> locals = local_decls(compiler);
    REQUIRE(locals.size() == 1);
    auto* q = std::get_if<VarDecl>(&ast.decl(locals[0]).node);
    REQUIRE(q != nullptr);
    CHECK(model.decl(locals[0]).scope == function_scope);
    CHECK(model.type_spec(q->spec).record == tag);
}

TEST_CASE("sema keeps tentative definitions on one entity", "[sema][architecture]") {
    const std::string source = R"(
struct S;
struct S g;
struct S g;
struct S { int x; };
)";
    TestCompiler compiler;
    std::string error;
    INFO(source);
    INFO(error);
    REQUIRE(compiler.analyze(source, &error));

    const AST& ast = compiler.ast();
    const SemanticModel& model = compiler.model();
    REQUIRE(ast.translation_unit().decls.size() == 4);
    DeclId tag_decl = top_level_decl(compiler, 0);
    DeclId first = top_level_decl(compiler, 1);
    DeclId second = top_level_decl(compiler, 2);
    DeclId completion = top_level_decl(compiler, 3);

    EntityId entity = model.decl(first).entity;
    REQUIRE(entity.valid());
    CHECK(model.decl(second).entity == entity);
    CHECK(model.decl(first).scope == model.file_scope());
    CHECK(model.decl(second).scope == model.file_scope());

    const Entity& object = model.entity(entity);
    CHECK(object.kind == EntityKind::Object);
    CHECK(object.linkage == Linkage::External);
    CHECK(object.definition == DefinitionState::Tentative);
    CHECK(object.declarations.size() == 2);
    REQUIRE(object.type != nullptr);
    CHECK(Type::is_struct(object.type));
    REQUIRE(object.type->record() != nullptr);
    CHECK(object.type->record()->complete);

    StructDecl* file_tag = model.decl(tag_decl).record;
    REQUIRE(file_tag != nullptr);
    CHECK(file_tag == object.type->record());
    CHECK(model.decl(completion).record == file_tag);
    CHECK(model.lookup_tag(model.file_scope(), file_tag->tag) == file_tag);

    Symbol name = decl_name(compiler, first);
    REQUIRE(name != nullptr);
    CHECK(model.lookup(model.file_scope(), name) == entity);
}

TEST_CASE("sema gives shadowed locals distinct entities and scopes", "[sema][architecture]") {
    const std::string source = "int main(void) { int x; { int x; x = 1; } return x; }";
    TestCompiler compiler;
    std::string error;
    INFO(source);
    INFO(error);
    REQUIRE(compiler.analyze(source, &error));

    const SemanticModel& model = compiler.model();
    std::vector<DeclId> locals = local_decls(compiler);
    REQUIRE(locals.size() == 2);
    EntityId outer = model.decl(locals[0]).entity;
    EntityId inner = model.decl(locals[1]).entity;
    REQUIRE(outer.valid());
    REQUIRE(inner.valid());
    CHECK(outer != inner);
    CHECK(model.entity(outer).declarations.size() == 1);
    CHECK(model.entity(inner).declarations.size() == 1);

    ScopeId outer_scope = model.decl(locals[0]).scope;
    ScopeId inner_scope = model.decl(locals[1]).scope;
    CHECK(model.scope(outer_scope).kind == ScopeKind::Function);
    CHECK(model.scope(inner_scope).kind == ScopeKind::Block);
    CHECK(model.scope(inner_scope).parent == outer_scope);

    Symbol name = decl_name(compiler, locals[0]);
    REQUIRE(name != nullptr);
    CHECK(model.binding(outer_scope, name) == outer);
    CHECK(model.binding(inner_scope, name) == inner);
    CHECK(model.lookup(inner_scope, name) == inner);
    CHECK(model.lookup(outer_scope, name) == outer);
}

TEST_CASE("sema gives shadowed struct tags distinct identities", "[sema][architecture]") {
    const std::string source = R"(
struct S { int a; };
int main(void) {
  struct S *p;
  {
    struct S;
    struct S *q;
    q = q;
  }
  p = p;
  return 0;
}
)";
    TestCompiler compiler;
    std::string error;
    INFO(source);
    INFO(error);
    REQUIRE(compiler.analyze(source, &error));

    const AST& ast = compiler.ast();
    const SemanticModel& model = compiler.model();
    REQUIRE(ast.translation_unit().decls.size() == 2);
    DeclId file_tag_decl = top_level_decl(compiler, 0);
    StructDecl* file_tag = model.decl(file_tag_decl).record;
    REQUIRE(file_tag != nullptr);
    CHECK(file_tag->complete);
    CHECK(model.binding_tag(model.file_scope(), file_tag->tag) == file_tag);

    std::vector<DeclId> locals = local_decls(compiler);
    REQUIRE(locals.size() == 3);
    auto* p = std::get_if<VarDecl>(&ast.decl(locals[0]).node);
    REQUIRE(p != nullptr);
    StructDecl* block_tag = model.decl(locals[1]).record;
    REQUIRE(block_tag != nullptr);
    CHECK(block_tag != file_tag);
    auto* q = std::get_if<VarDecl>(&ast.decl(locals[2]).node);
    REQUIRE(q != nullptr);

    ScopeId main_scope = model.decl(locals[0]).scope;
    ScopeId block_scope = model.decl(locals[1]).scope;
    CHECK(model.scope(main_scope).kind == ScopeKind::Function);
    CHECK(model.scope(block_scope).kind == ScopeKind::Block);
    CHECK(model.scope(block_scope).parent == main_scope);
    CHECK(model.type_spec(p->spec).record == file_tag);
    CHECK(model.type_spec(q->spec).record == block_tag);
    CHECK(model.binding_tag(block_scope, block_tag->tag) == block_tag);
    CHECK(model.lookup_tag(block_scope, block_tag->tag) == block_tag);
    CHECK(model.lookup_tag(main_scope, block_tag->tag) == file_tag);
}

TEST_CASE("sema binds labels in the function scope", "[sema][architecture]") {
    const std::string source = "int main(void) { goto end; end: return 0; }";
    TestCompiler compiler;
    std::string error;
    INFO(source);
    INFO(error);
    REQUIRE(compiler.analyze(source, &error));

    const AST& ast = compiler.ast();
    const SemanticModel& model = compiler.model();
    REQUIRE(ast.translation_unit().decls.size() == 1);
    auto* function = std::get_if<FunctionDecl>(&ast.decl(top_level_decl(compiler, 0)).node);
    REQUIRE(function != nullptr);
    auto* body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
    REQUIRE(body != nullptr);
    REQUIRE(body->items.size() == 2);
    auto* goto_stmt = std::get_if<GotoStmt>(&ast.stmt(body->items[0]).node);
    auto* label_stmt = std::get_if<LabelStmt>(&ast.stmt(body->items[1]).node);
    REQUIRE(goto_stmt != nullptr);
    REQUIRE(label_stmt != nullptr);

    ScopeId function_scope = only_function_scope(model);
    const LabelDecl& label = ast.label(label_stmt->label);
    CHECK(model.lookup_label(function_scope, label.name) == label_stmt->label);
    CHECK(model.lookup_label(model.file_scope(), label.name) == LabelId{});
    CHECK(model.goto_target(body->items[0]) == label_stmt->label);
    CHECK(model.label(label_stmt->label).defined);
}

TEST_CASE("sema treats only literal zero as a null pointer constant", "[sema][s009]") {
    auto assignment_rhs = [](const TestCompiler& compiler) -> ExprId {
        const AST& ast = compiler.ast();
        REQUIRE(ast.translation_unit().decls.size() == 1);
        const auto* function =
            std::get_if<FunctionDecl>(&ast.decl(ast.translation_unit().decls[0]).node);
        REQUIRE(function != nullptr);
        const auto* body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
        REQUIRE(body != nullptr);
        REQUIRE(body->items.size() >= 2);
        const auto* statement = std::get_if<ExprStmt>(&ast.stmt(body->items[1]).node);
        REQUIRE(statement != nullptr);
        const auto* assignment = std::get_if<AssignExpr>(&ast.expr(statement->expr).node);
        REQUIRE(assignment != nullptr);
        return assignment->rhs;
    };

    {
        TestCompiler compiler;
        std::string error;
        INFO(error);
        REQUIRE(compiler.analyze("int main(void) { int *p; p = 0; return 0; }", &error));
        const ExprInfo& info = compiler.model().expr(assignment_rhs(compiler));
        REQUIRE(info.constant() != nullptr);
        CHECK(info.constant()->value == 0);
        CHECK(info.is_null_pointer_constant());
    }

    {
        // (0) preserves the value but is not a null pointer constant.
        TestCompiler compiler;
        std::string error;
        INFO(error);
        REQUIRE(compiler.analyze("int main(void) { int x; x = (0); return x; }", &error));
        ExprId rhs = assignment_rhs(compiler);
        const ExprInfo& info = compiler.model().expr(rhs);
        REQUIRE(info.constant() != nullptr);
        CHECK(info.constant()->value == 0);
        CHECK_FALSE(info.is_null_pointer_constant());
        const auto* paren = std::get_if<ParenExpr>(&compiler.ast().expr(rhs).node);
        REQUIRE(paren != nullptr);
        CHECK(compiler.model().expr(paren->inner).is_null_pointer_constant());
    }

    {
        // Constant folding to zero must not create a null pointer constant.
        TestCompiler compiler;
        std::string error;
        INFO(error);
        REQUIRE(compiler.analyze("int main(void) { int x; x = 0 + 0; return x; }", &error));
        const ExprInfo& info = compiler.model().expr(assignment_rhs(compiler));
        REQUIRE(info.constant() != nullptr);
        CHECK(info.constant()->value == 0);
        CHECK_FALSE(info.is_null_pointer_constant());
    }

    {
        TestCompiler compiler;
        std::string error;
        INFO(error);
        REQUIRE(compiler.analyze("int main(void) { int x; x = -0; return x; }", &error));
        const ExprInfo& info = compiler.model().expr(assignment_rhs(compiler));
        REQUIRE(info.constant() != nullptr);
        CHECK(info.constant()->value == 0);
        CHECK_FALSE(info.is_null_pointer_constant());
    }

    require_valid("int main(void) { int *p; return p == 0; }");
    require_valid("int main(void) { int *p; int c; p = c ? p : 0; return 0; }");
    require_invalid("int main(void) { int *p; p = (0); return 0; }");
    require_invalid("int main(void) { int *p; p = 0 + 0; return 0; }");
    require_invalid("int main(void) { int *p; p = -0; return 0; }");
    require_invalid("int main(void) { int *p; return p == (0); }");
    require_invalid("int main(void) { int *p; int c; p = c ? p : (0); return 0; }");
}

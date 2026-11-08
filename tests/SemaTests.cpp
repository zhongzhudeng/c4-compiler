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
  requireValid("struct S { int a, b; }; "
               "int main(void) { struct S s; s.a = 1; s.b = 2; "
               "return s.a + s.b; }");
  requireValid("struct S { int *p, q; }; "
               "int main(void) { struct S s; int x; s.p = &x; s.q = 2; "
               "return s.q; }");
  requireValid("int f(struct S *p) { struct S *q; return 0; }");
  requireValid("struct S; int f(struct S *p); struct S { int x; }; "
               "int f(struct S *p) { return 0; }");
  requireValid("struct S { struct T { int x; } a, b; }; "
               "int main(void) { struct S s; s.a.x = 1; s.b.x = 2; "
               "return s.a.x + s.b.x; }");
  requireValid("int (*f(struct S *p))(int) { struct S *q; q = p; "
               "return 0; }");
  requireValid("struct S; int f(int (*g)(struct S *), struct S *p); "
               "struct S { int x; }; "
               "int f(int (*g)(struct S *), struct S *p) { return 0; }");
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
  requireInvalid("struct S { int a, a; }; int main(void) { return 0; }");
  requireInvalid("struct { int x; }; int main(void) { return 0; }");
  requireInvalid("int f(struct S *p); struct S { int x; }; "
                 "int f(struct S *p) { return 0; }");
  requireInvalid("int f(struct S *p); int f(struct S *p);");
  requireInvalid("int f(int (*g)(struct S *), struct S *p); "
                 "struct S { int x; }; "
                 "int f(int (*g)(struct S *), struct S *p) { return 0; }");
  // A tag in the return type's parameter list has prototype scope and must
  // not be visible in the function body.
  requireInvalid("int (*f(struct S *p))(struct T { int x; } *) { "
                 "struct T *r; r->x = 1; return 0; }");
  // A tag declared in a nested parameter declarator must not leak into the
  // enclosing function's body either.
  requireInvalid("int f(int g(struct T { int x; } *p)) { "
                 "struct T *r; r->x = 1; return 0; }");
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

TEST_CASE("sema scopes struct tags and completes tentative definitions",
          "[sema]") {
  // A block-scope "struct S;" declares a new tag instead of reusing the
  // outer definition, so the two pointer types are incompatible.
  requireInvalid(R"(
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
  requireInvalid(R"(
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
  requireValid(R"(
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
  requireValid("struct S; struct S g; struct S { int x; }; "
               "int main(void) { return sizeof(g); }");
  requireInvalid("struct S; struct S g; int main(void) { return 0; }");
  // The type is still incomplete at this use, even though it is completed
  // later in the translation unit.
  requireInvalid("struct S; struct S g; "
                 "int main(void) { return sizeof(g); } "
                 "struct S { int x; };");
  requireInvalid("struct S; int main(void) { struct S s; return 0; }");
}

TEST_CASE("sema lets block-scope functions hide outer block objects",
          "[sema]") {
  requireValid("int f(void) { return 42; } "
               "int main(void) { int f; { int f(void); return f(); } }");
  requireValid("int main(void) { int f; { int f(void); return 0; } }");
  // A file-scope object has external linkage, so it conflicts with the
  // block-scope function declaration.
  requireInvalid("int f; int main(void) { { int f(void); return 0; } }");
  requireInvalid("int f(void) { return 0; } "
                 "int main(void) { int f; { int f(int); return 0; } }");
}


namespace {

DeclId topLevelDecl(const TestCompiler &compiler, size_t index) {
  return compiler.ast().translationUnit().decls.at(index);
}

Symbol declName(const TestCompiler &compiler, DeclId id) {
  const AST &ast = compiler.ast();
  const Decl &decl = ast.decl(id);
  if (auto *var = std::get_if<VarDecl>(&decl.node))
    return declaratorName(ast, var->declarator);
  if (auto *function = std::get_if<FunctionDecl>(&decl.node))
    return declaratorName(ast, function->declarator);
  if (auto *param = std::get_if<ParamDecl>(&decl.node))
    return declaratorName(ast, param->declarator);
  if (auto *field = std::get_if<FieldDecl>(&decl.node))
    return declaratorName(ast, field->declarator);
  return Symbol();
}

void collectDeclaratorParams(const AST &ast, DeclaratorId id,
                             std::vector<DeclId> &out) {
  if (!id.valid())
    return;
  const Declarator &declarator = ast.declarator(id);
  if (auto *pointer = std::get_if<PointerDeclarator>(&declarator.node)) {
    collectDeclaratorParams(ast, pointer->inner, out);
    return;
  }
  if (auto *function = std::get_if<FunctionDeclarator>(&declarator.node)) {
    for (DeclId param : function->params)
      out.push_back(param);
    collectDeclaratorParams(ast, function->inner, out);
  }
}

std::vector<DeclId> declaratorParams(const AST &ast, DeclaratorId id) {
  std::vector<DeclId> result;
  collectDeclaratorParams(ast, id, result);
  return result;
}

void collectStmtDecls(const AST &ast, StmtId id, std::vector<DeclId> &out) {
  if (!id.valid())
    return;
  const Stmt &stmt = ast.stmt(id);
  if (auto *compound = std::get_if<CompoundStmt>(&stmt.node)) {
    for (StmtId item : compound->items)
      collectStmtDecls(ast, item, out);
    return;
  }
  if (auto *declStmt = std::get_if<DeclStmt>(&stmt.node)) {
    out.push_back(declStmt->decl);
    return;
  }
  if (auto *ifStmt = std::get_if<IfStmt>(&stmt.node)) {
    collectStmtDecls(ast, ifStmt->thenStmt, out);
    collectStmtDecls(ast, ifStmt->elseStmt, out);
    return;
  }
  if (auto *whileStmt = std::get_if<WhileStmt>(&stmt.node)) {
    collectStmtDecls(ast, whileStmt->body, out);
    return;
  }
  if (auto *labelStmt = std::get_if<LabelStmt>(&stmt.node))
    collectStmtDecls(ast, labelStmt->stmt, out);
}

std::vector<DeclId> localDecls(const TestCompiler &compiler) {
  std::vector<DeclId> result;
  const AST &ast = compiler.ast();
  for (DeclId id : ast.translationUnit().decls) {
    const Decl &decl = ast.decl(id);
    if (auto *function = std::get_if<FunctionDecl>(&decl.node))
      if (function->body.valid())
        collectStmtDecls(ast, function->body, result);
  }
  return result;
}

ScopeId onlyFunctionScope(const SemanticModel &model) {
  ScopeId result;
  for (size_t i = 0; i < model.scopeCount(); ++i) {
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

TEST_CASE("sema gives compatible declarations one entity",
          "[sema][architecture]") {
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

  const AST &ast = compiler.ast();
  const SemanticModel &model = compiler.model();
  REQUIRE(ast.translationUnit().decls.size() == 3);
  DeclId first = topLevelDecl(compiler, 0);
  DeclId definition = topLevelDecl(compiler, 1);
  DeclId last = topLevelDecl(compiler, 2);

  EntityId entity = model.decl(first).entity;
  REQUIRE(entity.valid());
  CHECK(model.decl(definition).entity == entity);
  CHECK(model.decl(last).entity == entity);
  CHECK(model.decl(first).scope == model.fileScope());
  CHECK(model.decl(definition).scope == model.fileScope());
  CHECK(model.decl(last).scope == model.fileScope());

  const Entity &function = model.entity(entity);
  CHECK(function.kind == EntityKind::Function);
  CHECK(function.linkage == Linkage::External);
  CHECK(function.definition == DefinitionState::Defined);
  CHECK(function.declarations.size() == 3);

  Symbol name = declName(compiler, first);
  REQUIRE(name != nullptr);
  CHECK(model.lookup(model.fileScope(), name) == entity);

  REQUIRE(function.parameters.size() == 1);
  DeclId parameter = function.parameters[0];
  EntityId parameterEntity = model.decl(parameter).entity;
  REQUIRE(parameterEntity.valid());
  CHECK(model.entity(parameterEntity).kind == EntityKind::Object);
  CHECK(model.entity(parameterEntity).linkage == Linkage::None);
  ScopeId functionScope = model.decl(parameter).scope;
  CHECK(model.scope(functionScope).kind == ScopeKind::Function);
  CHECK(model.scope(functionScope).parent == model.fileScope());
  Symbol parameterName = declName(compiler, parameter);
  REQUIRE(parameterName != nullptr);
  CHECK(model.lookup(functionScope, parameterName) == parameterEntity);
}

TEST_CASE("sema binds block-scope functions to the external entity",
          "[sema][architecture]") {
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

  const SemanticModel &model = compiler.model();
  std::vector<DeclId> locals = localDecls(compiler);
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

  ScopeId objectScope = model.decl(locals[0]).scope;
  ScopeId firstBlock = model.decl(locals[1]).scope;
  ScopeId secondBlock = model.decl(locals[2]).scope;
  CHECK(model.scope(objectScope).kind == ScopeKind::Function);
  CHECK(model.scope(firstBlock).kind == ScopeKind::Block);
  CHECK(model.scope(secondBlock).kind == ScopeKind::Block);
  CHECK(model.scope(firstBlock).parent == objectScope);
  CHECK(model.scope(secondBlock).parent == objectScope);
  CHECK(firstBlock != secondBlock);

  Symbol name = declName(compiler, locals[0]);
  REQUIRE(name != nullptr);
  CHECK(model.binding(objectScope, name) == object);
  CHECK(model.binding(firstBlock, name) == function);
  CHECK(model.binding(secondBlock, name) == function);
  CHECK(model.lookup(firstBlock, name) == function);
  CHECK(model.lookup(objectScope, name) == object);
}

TEST_CASE("sema keeps prototype tags in function prototype scope",
          "[sema][architecture]") {
  const std::string source = R"(
int f(struct P { int x; } *p);
int main(void) { struct P *q; return 0; }
)";
  TestCompiler compiler;
  std::string error;
  INFO(source);
  INFO(error);
  REQUIRE(compiler.analyze(source, &error));

  const AST &ast = compiler.ast();
  const SemanticModel &model = compiler.model();
  REQUIRE(ast.translationUnit().decls.size() == 2);
  auto *function =
      std::get_if<FunctionDecl>(&ast.decl(topLevelDecl(compiler, 0)).node);
  REQUIRE(function != nullptr);
  std::vector<DeclId> params = declaratorParams(ast, function->declarator);
  REQUIRE(params.size() == 1);
  DeclId parameter = params[0];

  ScopeId prototype = model.decl(parameter).scope;
  CHECK(model.scope(prototype).kind == ScopeKind::FunctionPrototype);
  auto *paramDecl = std::get_if<ParamDecl>(&ast.decl(parameter).node);
  REQUIRE(paramDecl != nullptr);
  const TypeSpec &spec = ast.typeSpec(paramDecl->spec);
  auto *structure = std::get_if<TypeSpec::Struct>(&spec.node);
  REQUIRE(structure != nullptr);
  REQUIRE(structure->tag != nullptr);
  StructDecl *prototypeTag = model.typeSpec(paramDecl->spec).record;
  REQUIRE(prototypeTag != nullptr);
  CHECK(model.bindingTag(prototype, structure->tag) == prototypeTag);
  CHECK(model.bindingTag(model.fileScope(), structure->tag) == nullptr);
  CHECK(model.lookupTag(model.fileScope(), structure->tag) == nullptr);

  std::vector<DeclId> locals = localDecls(compiler);
  REQUIRE(locals.size() == 1);
  auto *q = std::get_if<VarDecl>(&ast.decl(locals[0]).node);
  REQUIRE(q != nullptr);
  StructDecl *mainTag = model.typeSpec(q->spec).record;
  REQUIRE(mainTag != nullptr);
  CHECK(mainTag != prototypeTag);
  ScopeId mainScope = model.decl(locals[0]).scope;
  CHECK(model.scope(mainScope).kind == ScopeKind::Function);
  CHECK(model.bindingTag(mainScope, structure->tag) == mainTag);
  CHECK(model.lookupTag(mainScope, structure->tag) == mainTag);
}

TEST_CASE("sema keeps definition parameter tags in the function scope",
          "[sema][architecture]") {
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

  const AST &ast = compiler.ast();
  const SemanticModel &model = compiler.model();
  REQUIRE(ast.translationUnit().decls.size() == 1);
  DeclId definition = topLevelDecl(compiler, 0);
  auto *function = std::get_if<FunctionDecl>(&ast.decl(definition).node);
  REQUIRE(function != nullptr);
  std::vector<DeclId> params = declaratorParams(ast, function->declarator);
  REQUIRE(params.size() == 1);
  DeclId parameter = params[0];

  ScopeId functionScope = model.decl(parameter).scope;
  CHECK(model.scope(functionScope).kind == ScopeKind::Function);
  CHECK(model.scope(functionScope).parent == model.fileScope());
  auto *paramDecl = std::get_if<ParamDecl>(&ast.decl(parameter).node);
  REQUIRE(paramDecl != nullptr);
  const TypeSpec &spec = ast.typeSpec(paramDecl->spec);
  auto *structure = std::get_if<TypeSpec::Struct>(&spec.node);
  REQUIRE(structure != nullptr);
  REQUIRE(structure->tag != nullptr);
  StructDecl *tag = model.typeSpec(paramDecl->spec).record;
  REQUIRE(tag != nullptr);
  CHECK(tag->complete);
  CHECK(model.bindingTag(functionScope, structure->tag) == tag);
  CHECK(model.bindingTag(model.fileScope(), structure->tag) == nullptr);

  EntityId entityId = model.decl(definition).entity;
  REQUIRE(entityId.valid());
  const Entity &entity = model.entity(entityId);
  CHECK(entity.definition == DefinitionState::Defined);
  REQUIRE(entity.parameters.size() == 1);
  CHECK(entity.parameters[0] == parameter);
  CHECK(model.entity(model.decl(parameter).entity).linkage == Linkage::None);

  std::vector<DeclId> locals = localDecls(compiler);
  REQUIRE(locals.size() == 1);
  auto *q = std::get_if<VarDecl>(&ast.decl(locals[0]).node);
  REQUIRE(q != nullptr);
  CHECK(model.decl(locals[0]).scope == functionScope);
  CHECK(model.typeSpec(q->spec).record == tag);
  Symbol parameterName = declName(compiler, parameter);
  REQUIRE(parameterName != nullptr);
  CHECK(model.lookup(functionScope, parameterName) ==
        model.decl(parameter).entity);
}

TEST_CASE("sema adopts only the defining function declarator's scope",
          "[sema][architecture]") {
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

  const AST &ast = compiler.ast();
  const SemanticModel &model = compiler.model();
  REQUIRE(ast.translationUnit().decls.size() == 1);
  DeclId definition = topLevelDecl(compiler, 0);
  EntityId entityId = model.decl(definition).entity;
  REQUIRE(entityId.valid());
  const Entity &entity = model.entity(entityId);
  CHECK(entity.kind == EntityKind::Function);
  REQUIRE(entity.parameters.size() == 1);
  DeclId parameter = entity.parameters[0];

  ScopeId functionScope = model.decl(parameter).scope;
  CHECK(model.scope(functionScope).kind == ScopeKind::Function);
  CHECK(model.scope(functionScope).parent == model.fileScope());
  auto *paramDecl = std::get_if<ParamDecl>(&ast.decl(parameter).node);
  REQUIRE(paramDecl != nullptr);
  const TypeSpec &spec = ast.typeSpec(paramDecl->spec);
  auto *structure = std::get_if<TypeSpec::Struct>(&spec.node);
  REQUIRE(structure != nullptr);
  REQUIRE(structure->tag != nullptr);
  StructDecl *tag = model.typeSpec(paramDecl->spec).record;
  REQUIRE(tag != nullptr);
  CHECK(model.bindingTag(functionScope, structure->tag) == tag);
  CHECK(model.bindingTag(model.fileScope(), structure->tag) == nullptr);

  size_t prototypes = 0;
  for (size_t i = 0; i < model.scopeCount(); ++i) {
    ScopeId id{static_cast<uint32_t>(i)};
    if (model.scope(id).kind != ScopeKind::FunctionPrototype)
      continue;
    ++prototypes;
    CHECK(model.scope(id).tags.empty());
  }
  CHECK(prototypes == 1);

  std::vector<DeclId> locals = localDecls(compiler);
  REQUIRE(locals.size() == 1);
  auto *q = std::get_if<VarDecl>(&ast.decl(locals[0]).node);
  REQUIRE(q != nullptr);
  CHECK(model.decl(locals[0]).scope == functionScope);
  CHECK(model.typeSpec(q->spec).record == tag);
}

TEST_CASE("sema keeps tentative definitions on one entity",
          "[sema][architecture]") {
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

  const AST &ast = compiler.ast();
  const SemanticModel &model = compiler.model();
  REQUIRE(ast.translationUnit().decls.size() == 4);
  DeclId tagDecl = topLevelDecl(compiler, 0);
  DeclId first = topLevelDecl(compiler, 1);
  DeclId second = topLevelDecl(compiler, 2);
  DeclId completion = topLevelDecl(compiler, 3);

  EntityId entity = model.decl(first).entity;
  REQUIRE(entity.valid());
  CHECK(model.decl(second).entity == entity);
  CHECK(model.decl(first).scope == model.fileScope());
  CHECK(model.decl(second).scope == model.fileScope());

  const Entity &object = model.entity(entity);
  CHECK(object.kind == EntityKind::Object);
  CHECK(object.linkage == Linkage::External);
  CHECK(object.definition == DefinitionState::Tentative);
  CHECK(object.declarations.size() == 2);
  REQUIRE(object.type != nullptr);
  CHECK(Type::isStruct(object.type));
  REQUIRE(object.type->record() != nullptr);
  CHECK(object.type->record()->complete);

  StructDecl *fileTag = model.decl(tagDecl).record;
  REQUIRE(fileTag != nullptr);
  CHECK(fileTag == object.type->record());
  CHECK(model.decl(completion).record == fileTag);
  CHECK(model.lookupTag(model.fileScope(), fileTag->tag) == fileTag);

  Symbol name = declName(compiler, first);
  REQUIRE(name != nullptr);
  CHECK(model.lookup(model.fileScope(), name) == entity);
}

TEST_CASE("sema gives shadowed locals distinct entities and scopes",
          "[sema][architecture]") {
  const std::string source =
      "int main(void) { int x; { int x; x = 1; } return x; }";
  TestCompiler compiler;
  std::string error;
  INFO(source);
  INFO(error);
  REQUIRE(compiler.analyze(source, &error));

  const SemanticModel &model = compiler.model();
  std::vector<DeclId> locals = localDecls(compiler);
  REQUIRE(locals.size() == 2);
  EntityId outer = model.decl(locals[0]).entity;
  EntityId inner = model.decl(locals[1]).entity;
  REQUIRE(outer.valid());
  REQUIRE(inner.valid());
  CHECK(outer != inner);
  CHECK(model.entity(outer).declarations.size() == 1);
  CHECK(model.entity(inner).declarations.size() == 1);

  ScopeId outerScope = model.decl(locals[0]).scope;
  ScopeId innerScope = model.decl(locals[1]).scope;
  CHECK(model.scope(outerScope).kind == ScopeKind::Function);
  CHECK(model.scope(innerScope).kind == ScopeKind::Block);
  CHECK(model.scope(innerScope).parent == outerScope);

  Symbol name = declName(compiler, locals[0]);
  REQUIRE(name != nullptr);
  CHECK(model.binding(outerScope, name) == outer);
  CHECK(model.binding(innerScope, name) == inner);
  CHECK(model.lookup(innerScope, name) == inner);
  CHECK(model.lookup(outerScope, name) == outer);
}

TEST_CASE("sema gives shadowed struct tags distinct identities",
          "[sema][architecture]") {
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

  const AST &ast = compiler.ast();
  const SemanticModel &model = compiler.model();
  REQUIRE(ast.translationUnit().decls.size() == 2);
  DeclId fileTagDecl = topLevelDecl(compiler, 0);
  StructDecl *fileTag = model.decl(fileTagDecl).record;
  REQUIRE(fileTag != nullptr);
  CHECK(fileTag->complete);
  CHECK(model.bindingTag(model.fileScope(), fileTag->tag) == fileTag);

  std::vector<DeclId> locals = localDecls(compiler);
  REQUIRE(locals.size() == 3);
  auto *p = std::get_if<VarDecl>(&ast.decl(locals[0]).node);
  REQUIRE(p != nullptr);
  StructDecl *blockTag = model.decl(locals[1]).record;
  REQUIRE(blockTag != nullptr);
  CHECK(blockTag != fileTag);
  auto *q = std::get_if<VarDecl>(&ast.decl(locals[2]).node);
  REQUIRE(q != nullptr);

  ScopeId mainScope = model.decl(locals[0]).scope;
  ScopeId blockScope = model.decl(locals[1]).scope;
  CHECK(model.scope(mainScope).kind == ScopeKind::Function);
  CHECK(model.scope(blockScope).kind == ScopeKind::Block);
  CHECK(model.scope(blockScope).parent == mainScope);
  CHECK(model.typeSpec(p->spec).record == fileTag);
  CHECK(model.typeSpec(q->spec).record == blockTag);
  CHECK(model.bindingTag(blockScope, blockTag->tag) == blockTag);
  CHECK(model.lookupTag(blockScope, blockTag->tag) == blockTag);
  CHECK(model.lookupTag(mainScope, blockTag->tag) == fileTag);
}

TEST_CASE("sema binds labels in the function scope", "[sema][architecture]") {
  const std::string source =
      "int main(void) { goto end; end: return 0; }";
  TestCompiler compiler;
  std::string error;
  INFO(source);
  INFO(error);
  REQUIRE(compiler.analyze(source, &error));

  const AST &ast = compiler.ast();
  const SemanticModel &model = compiler.model();
  REQUIRE(ast.translationUnit().decls.size() == 1);
  auto *function =
      std::get_if<FunctionDecl>(&ast.decl(topLevelDecl(compiler, 0)).node);
  REQUIRE(function != nullptr);
  auto *body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
  REQUIRE(body != nullptr);
  REQUIRE(body->items.size() == 2);
  auto *gotoStmt = std::get_if<GotoStmt>(&ast.stmt(body->items[0]).node);
  auto *labelStmt = std::get_if<LabelStmt>(&ast.stmt(body->items[1]).node);
  REQUIRE(gotoStmt != nullptr);
  REQUIRE(labelStmt != nullptr);

  ScopeId functionScope = onlyFunctionScope(model);
  const LabelDecl &label = ast.label(labelStmt->label);
  CHECK(model.lookupLabel(functionScope, label.name) == labelStmt->label);
  CHECK(model.lookupLabel(model.fileScope(), label.name) == LabelId{});
  CHECK(model.gotoTarget(body->items[0]) == labelStmt->label);
  CHECK(model.label(labelStmt->label).defined);
}

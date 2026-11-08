#include "TestSupport.hpp"

#include <catch2/catch_test_macros.hpp>

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"

#include <string>

namespace {

const Expr *firstReturnExpr(const AST &ast) {
  for (DeclId declaration : ast.translationUnit().decls) {
    auto *function = std::get_if<FunctionDecl>(&ast.decl(declaration).node);
    if (!function || !function->body.valid())
      continue;
    auto *body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
    if (!body)
      continue;
    for (StmtId item : body->items) {
      if (auto *returnStmt = std::get_if<ReturnStmt>(&ast.stmt(item).node))
        return &ast.expr(returnStmt->expr);
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("TypeContext interns canonical primitive and pointer types",
          "[type]") {
  TypeContext types;

  CHECK(types.voidType() == types.voidType());
  CHECK(types.charType() == types.charType());
  CHECK(types.intType() == types.intType());
  CHECK(types.voidType() != types.charType());
  CHECK(types.charType() != types.intType());

  CHECK(types.voidType()->is<Type::Void>());
  CHECK(types.charType()->is<Type::Char>());
  CHECK(types.intType()->is<Type::Int>());

  const Type *intPointer = types.pointerTo(types.intType());
  CHECK(intPointer == types.pointerTo(types.intType()));
  CHECK(intPointer != types.pointerTo(types.charType()));
  CHECK(intPointer->is<Type::Pointer>());
  CHECK(intPointer->pointee() == types.intType());

  const Type *pointerPointer = types.pointerTo(intPointer);
  CHECK(pointerPointer == types.pointerTo(intPointer));
  CHECK(pointerPointer != intPointer);
  CHECK(pointerPointer->pointee() == intPointer);
  CHECK(pointerPointer->pointee()->pointee() == types.intType());

  // A category-specific accessor cannot read an unrelated alternative.
  CHECK_THROWS_AS(types.intType()->pointee(), std::bad_variant_access);

  // Canonical addresses remain stable while more types are interned.
  for (unsigned i = 0; i < 512; ++i)
    types.pointerTo(types.intType());
  CHECK(types.pointerTo(types.intType()) == intPointer);
  CHECK(intPointer->pointee() == types.intType());
}

TEST_CASE("TypeContext interns function and array types structurally",
          "[type]") {
  TypeContext types;
  const Type *intChar =
      types.functionOf(types.intType(), {types.intType(), types.charType()});

  CHECK(intChar ==
        types.functionOf(types.intType(), {types.intType(), types.charType()}));
  CHECK(intChar->is<Type::Function>());
  CHECK(intChar->result() == types.intType());
  REQUIRE(intChar->params().size() == 2);
  CHECK(intChar->params()[0] == types.intType());
  CHECK(intChar->params()[1] == types.charType());

  CHECK(intChar !=
        types.functionOf(types.intType(), {types.charType(), types.intType()}));
  CHECK(intChar != types.functionOf(types.intType(), {types.intType()}));
  CHECK(intChar !=
        types.functionOf(types.charType(), {types.intType(), types.charType()}));
  CHECK(intChar !=
        types.functionOf(types.intType(), {types.intType(), types.charType(),
                                           types.intType()}));

  const Type *functionPointer = types.pointerTo(intChar);
  CHECK(functionPointer == types.pointerTo(intChar));
  CHECK(functionPointer->pointee() == intChar);
  CHECK(functionPointer->pointee()->result() == types.intType());

  const Type *charArray = types.arrayOf(types.charType(), 3);
  CHECK(charArray == types.arrayOf(types.charType(), 3));
  CHECK(charArray != types.arrayOf(types.charType(), 4));
  CHECK(charArray != types.arrayOf(types.intType(), 3));
  CHECK(charArray->is<Type::Array>());
  CHECK(charArray->element() == types.charType());
  CHECK(charArray->count() == 3);
  CHECK(types.arrayOf(charArray, 2)->element() == charArray);
}

TEST_CASE("TypeContext preserves struct identity across completion",
          "[type]") {
  TypeContext types;
  std::string tag = "Node";

  StructDecl node;
  node.tag = &tag;
  const Type *incomplete = types.structType(&node);
  CHECK(incomplete == types.structType(&node));
  CHECK(incomplete->is<Type::Struct>());
  CHECK(incomplete->record() == &node);
  CHECK(Type::isStruct(incomplete));
  CHECK(Type::isObject(incomplete));
  CHECK(TypeContext::toString(incomplete) == "struct Node");

  node.complete = true;
  node.type = incomplete;
  CHECK(types.structType(&node) == incomplete);

  // Recursion through a pointer keeps the canonical struct type.
  node.fields.push_back(
      FieldInfo{nullptr, types.pointerTo(incomplete), DeclId{}, 0});
  CHECK(node.fields[0].type->pointee() == incomplete);
  CHECK(types.pointerTo(incomplete) ==
        types.pointerTo(types.structType(&node)));

  // Distinct tags, including shadowed ones, have distinct identities.
  std::string otherTag = "Node";
  StructDecl other;
  other.tag = &otherTag;
  CHECK(types.structType(&other) != incomplete);
  CHECK(types.pointerTo(types.structType(&other)) !=
        types.pointerTo(incomplete));

  StructDecl anonymous;
  CHECK(TypeContext::toString(types.structType(&anonymous)) ==
        "struct <anonymous>");
}

TEST_CASE("Type predicates and printing follow the active alternative",
          "[type]") {
  TypeContext types;
  const Type *intPointer = types.pointerTo(types.intType());
  const Type *charArray = types.arrayOf(types.charType(), 4);
  const Type *function = types.functionOf(types.voidType(), {});
  StructDecl record;
  const Type *structure = types.structType(&record);

  CHECK(Type::isVoid(types.voidType()));
  CHECK(Type::isChar(types.charType()));
  CHECK(Type::isInteger(types.charType()));
  CHECK(Type::isInteger(types.intType()));
  CHECK_FALSE(Type::isInteger(intPointer));
  CHECK(Type::isPointer(intPointer));
  CHECK(Type::isArray(charArray));
  CHECK(Type::isFunction(function));
  CHECK(Type::isStruct(structure));
  CHECK(Type::isScalar(types.intType()));
  CHECK(Type::isScalar(intPointer));
  CHECK_FALSE(Type::isScalar(charArray));
  CHECK_FALSE(Type::isScalar(function));
  CHECK(Type::isObject(types.charType()));
  CHECK(Type::isObject(intPointer));
  CHECK(Type::isObject(charArray));
  CHECK(Type::isObject(structure));
  CHECK_FALSE(Type::isObject(types.voidType()));
  CHECK_FALSE(Type::isObject(function));
  CHECK_FALSE(Type::isVoid(nullptr));

  CHECK(TypeContext::toString(nullptr) == "<null>");
  CHECK(TypeContext::toString(types.intType()) == "int");
  CHECK(TypeContext::toString(intPointer) == "int *");
  CHECK(TypeContext::toString(charArray) == "char[4]");
  CHECK(TypeContext::toString(function) == "void (void)");
  CHECK(TypeContext::toString(
            types.functionOf(types.intType(),
                             {types.intType(), types.charType()})) ==
        "int (int, char)");
  CHECK(TypeContext::toString(types.pointerTo(function)) == "void (void) *");
}

TEST_CASE("TypeLayout maps primitive, pointer, array, and struct types",
          "[type]") {
  llvm::LLVMContext context;
  llvm::DataLayout dataLayout("e-m:o-i64:64-i128:128-n32:64-S128");
  TypeLayout layout(context, dataLayout);
  TypeContext types;

  CHECK(layout.sizeOf(types.charType()) == 1);
  CHECK(layout.sizeOf(types.intType()) == 4);
  CHECK(layout.alignOf(types.intType()) == 4);
  CHECK(layout.sizeOf(types.pointerTo(types.intType())) == 8);
  CHECK(layout.sizeOf(types.arrayOf(types.intType(), 3)) == 12);

  StructDecl record;
  record.complete = true;
  const Type *recordType = types.structType(&record);
  record.type = recordType;
  record.fields.push_back(
      FieldInfo{nullptr, types.charType(), DeclId{}, 0});
  record.fields.push_back(
      FieldInfo{nullptr, types.intType(), DeclId{}, 0});

  CHECK(layout.sizeOf(recordType) == 8);
  CHECK(layout.fieldOffset(&record, 0) == 0);
  CHECK(layout.fieldOffset(&record, 1) == 4);
}

TEST_CASE("sizeof AST distinguishes expression and type-name forms",
          "[parser][type][ast]") {
  TestCompiler expressionCompiler;
  REQUIRE(expressionCompiler.parse(
      "int main(void) { int x; return sizeof x; }"));
  const Expr *expressionForm = firstReturnExpr(expressionCompiler.ast());
  REQUIRE(expressionForm != nullptr);
  auto *expressionSizeof = std::get_if<SizeofExpr>(&expressionForm->node);
  REQUIRE(expressionSizeof != nullptr);
  REQUIRE(std::holds_alternative<ExprId>(expressionSizeof->target));
  CHECK(std::get<ExprId>(expressionSizeof->target).valid());

  TestCompiler abstractCompiler;
  REQUIRE(abstractCompiler.parse(
      "int main(void) { return sizeof(int); }"));
  const Expr *abstractForm = firstReturnExpr(abstractCompiler.ast());
  REQUIRE(abstractForm != nullptr);
  auto *abstractSizeof = std::get_if<SizeofExpr>(&abstractForm->node);
  REQUIRE(abstractSizeof != nullptr);
  auto *abstractTypeName =
      std::get_if<SizeofExpr::TypeName>(&abstractSizeof->target);
  REQUIRE(abstractTypeName != nullptr);
  CHECK(abstractTypeName->spec.valid());
  CHECK_FALSE(abstractTypeName->declarator.valid());

  TestCompiler pointerCompiler;
  REQUIRE(pointerCompiler.parse(
      "int main(void) { return sizeof(int *); }"));
  const Expr *pointerForm = firstReturnExpr(pointerCompiler.ast());
  REQUIRE(pointerForm != nullptr);
  auto *pointerSizeof = std::get_if<SizeofExpr>(&pointerForm->node);
  REQUIRE(pointerSizeof != nullptr);
  auto *pointerTypeName =
      std::get_if<SizeofExpr::TypeName>(&pointerSizeof->target);
  REQUIRE(pointerTypeName != nullptr);
  REQUIRE(pointerTypeName->declarator.valid());
  CHECK(std::holds_alternative<PointerDeclarator>(
      pointerCompiler.ast().declarator(pointerTypeName->declarator).node));

  TestCompiler structCompiler;
  REQUIRE(structCompiler.parse(
      "struct S { int x; }; int main(void) { return sizeof(struct S); }"));
  const Expr *structForm = firstReturnExpr(structCompiler.ast());
  REQUIRE(structForm != nullptr);
  auto *structSizeof = std::get_if<SizeofExpr>(&structForm->node);
  REQUIRE(structSizeof != nullptr);
  auto *structTypeName =
      std::get_if<SizeofExpr::TypeName>(&structSizeof->target);
  REQUIRE(structTypeName != nullptr);
  CHECK_FALSE(structTypeName->declarator.valid());
  auto *structSpec = std::get_if<TypeSpec::Struct>(
      &structCompiler.ast().typeSpec(structTypeName->spec).node);
  REQUIRE(structSpec != nullptr);
  REQUIRE(structSpec->tag != nullptr);
  CHECK(*structSpec->tag == "S");
  CHECK_FALSE(structSpec->hasBraces);
}

TEST_CASE("printer renders both sizeof forms", "[printer][ast]") {
  TestCompiler compiler;
  std::string output;
  REQUIRE(compiler.print(
      "struct S { int x; }; "
      "int main(void) { int x; "
      "return sizeof x + sizeof(int) + sizeof(int *) + sizeof(struct S); }",
      &output));
  CHECK(output.find("(sizeof x)") != std::string::npos);
  CHECK(output.find("(sizeof(int))") != std::string::npos);
  CHECK(output.find("(sizeof(int *))") != std::string::npos);
  CHECK(output.find("(sizeof(struct S))") != std::string::npos);
}

TEST_CASE("TypeSpec alternatives encode primitive and struct syntax",
          "[parser][ast]") {
  {
    TestCompiler compiler;
    REQUIRE(compiler.parse("int main(void) { return 0; }"));
    auto *function = std::get_if<FunctionDecl>(
        &compiler.ast().decl(compiler.ast().translationUnit().decls[0]).node);
    REQUIRE(function != nullptr);
    CHECK(std::holds_alternative<TypeSpec::Int>(
        compiler.ast().typeSpec(function->spec).node));
  }

  {
    TestCompiler compiler;
    REQUIRE(compiler.parse("struct S { int x; };"));
    auto *tag = std::get_if<TagDecl>(
        &compiler.ast().decl(compiler.ast().translationUnit().decls[0]).node);
    REQUIRE(tag != nullptr);
    const TypeSpec &spec = compiler.ast().typeSpec(tag->spec);
    auto *structure = std::get_if<TypeSpec::Struct>(&spec.node);
    REQUIRE(structure != nullptr);
    REQUIRE(structure->tag != nullptr);
    CHECK(*structure->tag == "S");
    CHECK(structure->hasBraces);
    CHECK(structure->fields.size() == 1);
  }

  {
    TestCompiler compiler;
    REQUIRE(compiler.parse("struct S;"));
    auto *tag = std::get_if<TagDecl>(
        &compiler.ast().decl(compiler.ast().translationUnit().decls[0]).node);
    REQUIRE(tag != nullptr);
    const TypeSpec &spec = compiler.ast().typeSpec(tag->spec);
    auto *structure = std::get_if<TypeSpec::Struct>(&spec.node);
    REQUIRE(structure != nullptr);
    REQUIRE(structure->tag != nullptr);
    CHECK(*structure->tag == "S");
    CHECK_FALSE(structure->hasBraces);
    CHECK(structure->fields.empty());
  }

  {
    TestCompiler compiler;
    REQUIRE(compiler.parse("struct { int x; } s;"));
    auto *variable = std::get_if<VarDecl>(
        &compiler.ast().decl(compiler.ast().translationUnit().decls[0]).node);
    REQUIRE(variable != nullptr);
    const TypeSpec &spec = compiler.ast().typeSpec(variable->spec);
    auto *structure = std::get_if<TypeSpec::Struct>(&spec.node);
    REQUIRE(structure != nullptr);
    CHECK(structure->tag == nullptr);
    CHECK(structure->hasBraces);
    CHECK(structure->fields.size() == 1);
  }
}

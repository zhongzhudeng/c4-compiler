#include "TestSupport.hpp"

#include <catch2/catch_test_macros.hpp>

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"

#include <string>

namespace {

const Expr* first_return_expr(const AST& ast) {
    for (DeclId declaration : ast.translation_unit().decls) {
        auto* function = std::get_if<FunctionDecl>(&ast.decl(declaration).node);
        if (!function || !function->body.valid())
            continue;
        auto* body = std::get_if<CompoundStmt>(&ast.stmt(function->body).node);
        if (!body)
            continue;
        for (StmtId item : body->items) {
            if (auto* return_stmt = std::get_if<ReturnStmt>(&ast.stmt(item).node))
                return &ast.expr(return_stmt->expr);
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("TypeContext interns canonical primitive and pointer types", "[type]") {
    TypeContext types;

    CHECK(types.void_type() == types.void_type());
    CHECK(types.char_type() == types.char_type());
    CHECK(types.int_type() == types.int_type());
    CHECK(types.void_type() != types.char_type());
    CHECK(types.char_type() != types.int_type());

    CHECK(types.void_type()->is<Type::Void>());
    CHECK(types.char_type()->is<Type::Char>());
    CHECK(types.int_type()->is<Type::Int>());

    const Type* int_pointer = types.pointer_to(types.int_type());
    CHECK(int_pointer == types.pointer_to(types.int_type()));
    CHECK(int_pointer != types.pointer_to(types.char_type()));
    CHECK(int_pointer->is<Type::Pointer>());
    CHECK(int_pointer->pointee() == types.int_type());

    const Type* pointer_pointer = types.pointer_to(int_pointer);
    CHECK(pointer_pointer == types.pointer_to(int_pointer));
    CHECK(pointer_pointer != int_pointer);
    CHECK(pointer_pointer->pointee() == int_pointer);
    CHECK(pointer_pointer->pointee()->pointee() == types.int_type());

    // A category-specific accessor cannot read an unrelated alternative.
    CHECK_THROWS_AS(types.int_type()->pointee(), std::bad_variant_access);

    // Canonical addresses remain stable while more types are interned.
    for (unsigned i = 0; i < 512; ++i)
        types.pointer_to(types.int_type());
    CHECK(types.pointer_to(types.int_type()) == int_pointer);
    CHECK(int_pointer->pointee() == types.int_type());
}

TEST_CASE("TypeContext interns function and array types structurally", "[type]") {
    TypeContext types;
    const Type* int_char =
        types.function_of(types.int_type(), {types.int_type(), types.char_type()});

    CHECK(int_char == types.function_of(types.int_type(), {types.int_type(), types.char_type()}));
    CHECK(int_char->is<Type::Function>());
    CHECK(int_char->result() == types.int_type());
    REQUIRE(int_char->params().size() == 2);
    CHECK(int_char->params()[0] == types.int_type());
    CHECK(int_char->params()[1] == types.char_type());

    CHECK(int_char != types.function_of(types.int_type(), {types.char_type(), types.int_type()}));
    CHECK(int_char != types.function_of(types.int_type(), {types.int_type()}));
    CHECK(int_char != types.function_of(types.char_type(), {types.int_type(), types.char_type()}));
    CHECK(int_char != types.function_of(types.int_type(),
                                        {types.int_type(), types.char_type(), types.int_type()}));

    const Type* function_pointer = types.pointer_to(int_char);
    CHECK(function_pointer == types.pointer_to(int_char));
    CHECK(function_pointer->pointee() == int_char);
    CHECK(function_pointer->pointee()->result() == types.int_type());

    const Type* char_array = types.array_of(types.char_type(), 3);
    CHECK(char_array == types.array_of(types.char_type(), 3));
    CHECK(char_array != types.array_of(types.char_type(), 4));
    CHECK(char_array != types.array_of(types.int_type(), 3));
    CHECK(char_array->is<Type::Array>());
    CHECK(char_array->element() == types.char_type());
    CHECK(char_array->count() == 3);
    CHECK(types.array_of(char_array, 2)->element() == char_array);
}

TEST_CASE("TypeContext preserves struct identity across completion", "[type]") {
    TypeContext types;
    SymbolInternalizer symbols;

    StructDecl node;
    node.tag = symbols.internalize("Node");
    const Type* incomplete = types.struct_type(&node);
    CHECK(incomplete == types.struct_type(&node));
    CHECK(incomplete->is<Type::Struct>());
    CHECK(incomplete->record() == &node);
    CHECK(Type::is_struct(incomplete));
    CHECK(Type::is_object(incomplete));
    CHECK(TypeContext::to_string(incomplete) == "struct Node");

    node.complete = true;
    node.type = incomplete;
    CHECK(types.struct_type(&node) == incomplete);

    // Recursion through a pointer keeps the canonical struct type.
    node.fields.push_back(FieldInfo{Symbol(), types.pointer_to(incomplete), DeclId{}, 0});
    CHECK(node.fields[0].type->pointee() == incomplete);
    CHECK(types.pointer_to(incomplete) == types.pointer_to(types.struct_type(&node)));

    // Distinct tags, including shadowed ones, have distinct identities.
    StructDecl other;
    other.tag = symbols.internalize("Node");
    CHECK(types.struct_type(&other) != incomplete);
    CHECK(types.pointer_to(types.struct_type(&other)) != types.pointer_to(incomplete));

    StructDecl anonymous;
    CHECK(TypeContext::to_string(types.struct_type(&anonymous)) == "struct <anonymous>");
}

TEST_CASE("Type predicates and printing follow the active alternative", "[type]") {
    TypeContext types;
    const Type* int_pointer = types.pointer_to(types.int_type());
    const Type* char_array = types.array_of(types.char_type(), 4);
    const Type* function = types.function_of(types.void_type(), {});
    StructDecl record;
    const Type* structure = types.struct_type(&record);

    CHECK(Type::is_void(types.void_type()));
    CHECK(Type::is_char(types.char_type()));
    CHECK(Type::is_integer(types.char_type()));
    CHECK(Type::is_integer(types.int_type()));
    CHECK_FALSE(Type::is_integer(int_pointer));
    CHECK(Type::is_pointer(int_pointer));
    CHECK(Type::is_array(char_array));
    CHECK(Type::is_function(function));
    CHECK(Type::is_struct(structure));
    CHECK(Type::is_scalar(types.int_type()));
    CHECK(Type::is_scalar(int_pointer));
    CHECK_FALSE(Type::is_scalar(char_array));
    CHECK_FALSE(Type::is_scalar(function));
    CHECK(Type::is_object(types.char_type()));
    CHECK(Type::is_object(int_pointer));
    CHECK(Type::is_object(char_array));
    CHECK(Type::is_object(structure));
    CHECK_FALSE(Type::is_object(types.void_type()));
    CHECK_FALSE(Type::is_object(function));
    CHECK_FALSE(Type::is_void(nullptr));

    CHECK(TypeContext::to_string(nullptr) == "<null>");
    CHECK(TypeContext::to_string(types.int_type()) == "int");
    CHECK(TypeContext::to_string(int_pointer) == "int *");
    CHECK(TypeContext::to_string(char_array) == "char[4]");
    CHECK(TypeContext::to_string(function) == "void (void)");
    CHECK(TypeContext::to_string(types.function_of(
              types.int_type(), {types.int_type(), types.char_type()})) == "int (int, char)");
    CHECK(TypeContext::to_string(types.pointer_to(function)) == "void (void) *");
}

TEST_CASE("TypeLayout maps primitive, pointer, array, and struct types", "[type]") {
    llvm::LLVMContext context;
    llvm::DataLayout data_layout("e-m:o-i64:64-i128:128-n32:64-S128");
    TypeLayout layout(context, data_layout);
    TypeContext types;

    CHECK(layout.size_of(types.char_type()) == 1);
    CHECK(layout.size_of(types.int_type()) == 4);
    CHECK(layout.align_of(types.int_type()) == 4);
    CHECK(layout.size_of(types.pointer_to(types.int_type())) == 8);
    CHECK(layout.size_of(types.array_of(types.int_type(), 3)) == 12);

    StructDecl record;
    record.complete = true;
    const Type* record_type = types.struct_type(&record);
    record.type = record_type;
    record.fields.push_back(FieldInfo{Symbol(), types.char_type(), DeclId{}, 0});
    record.fields.push_back(FieldInfo{Symbol(), types.int_type(), DeclId{}, 0});

    CHECK(layout.size_of(record_type) == 8);
    CHECK(layout.field_offset(&record, 0) == 0);
    CHECK(layout.field_offset(&record, 1) == 4);
}

TEST_CASE("sizeof AST distinguishes expression and type-name forms", "[parser][type][ast]") {
    TestCompiler expression_compiler;
    REQUIRE(expression_compiler.parse("int main(void) { int x; return sizeof x; }"));
    const Expr* expression_form = first_return_expr(expression_compiler.ast());
    REQUIRE(expression_form != nullptr);
    auto* expression_sizeof = std::get_if<SizeofExpr>(&expression_form->node);
    REQUIRE(expression_sizeof != nullptr);
    REQUIRE(std::holds_alternative<ExprId>(expression_sizeof->target));
    CHECK(std::get<ExprId>(expression_sizeof->target).valid());

    TestCompiler abstract_compiler;
    REQUIRE(abstract_compiler.parse("int main(void) { return sizeof(int); }"));
    const Expr* abstract_form = first_return_expr(abstract_compiler.ast());
    REQUIRE(abstract_form != nullptr);
    auto* abstract_sizeof = std::get_if<SizeofExpr>(&abstract_form->node);
    REQUIRE(abstract_sizeof != nullptr);
    auto* abstract_type_name = std::get_if<SizeofExpr::TypeName>(&abstract_sizeof->target);
    REQUIRE(abstract_type_name != nullptr);
    CHECK(abstract_type_name->spec.valid());
    CHECK_FALSE(abstract_type_name->declarator.valid());

    TestCompiler pointer_compiler;
    REQUIRE(pointer_compiler.parse("int main(void) { return sizeof(int *); }"));
    const Expr* pointer_form = first_return_expr(pointer_compiler.ast());
    REQUIRE(pointer_form != nullptr);
    auto* pointer_sizeof = std::get_if<SizeofExpr>(&pointer_form->node);
    REQUIRE(pointer_sizeof != nullptr);
    auto* pointer_type_name = std::get_if<SizeofExpr::TypeName>(&pointer_sizeof->target);
    REQUIRE(pointer_type_name != nullptr);
    REQUIRE(pointer_type_name->declarator.valid());
    CHECK(std::holds_alternative<PointerDeclarator>(
        pointer_compiler.ast().declarator(pointer_type_name->declarator).node));

    TestCompiler struct_compiler;
    REQUIRE(
        struct_compiler.parse("struct S { int x; }; int main(void) { return sizeof(struct S); }"));
    const Expr* struct_form = first_return_expr(struct_compiler.ast());
    REQUIRE(struct_form != nullptr);
    auto* struct_sizeof = std::get_if<SizeofExpr>(&struct_form->node);
    REQUIRE(struct_sizeof != nullptr);
    auto* struct_type_name = std::get_if<SizeofExpr::TypeName>(&struct_sizeof->target);
    REQUIRE(struct_type_name != nullptr);
    CHECK_FALSE(struct_type_name->declarator.valid());
    auto* struct_spec = std::get_if<TypeSpec::StructReference>(
        &struct_compiler.ast().type_spec(struct_type_name->spec).node);
    REQUIRE(struct_spec != nullptr);
    REQUIRE(struct_spec->tag != nullptr);
    CHECK(*struct_spec->tag == "S");
}

TEST_CASE("printer renders both sizeof forms", "[printer][ast]") {
    TestCompiler compiler;
    std::string output;
    REQUIRE(compiler.print("struct S { int x; }; "
                           "int main(void) { int x; "
                           "return sizeof x + sizeof(int) + sizeof(int *) + sizeof(struct S); }",
                           &output));
    CHECK(output.find("(sizeof x)") != std::string::npos);
    CHECK(output.find("(sizeof(int))") != std::string::npos);
    CHECK(output.find("(sizeof(int *))") != std::string::npos);
    CHECK(output.find("(sizeof(struct S))") != std::string::npos);
}

TEST_CASE("TypeSpec alternatives encode primitive and struct syntax", "[parser][ast]") {
    {
        TestCompiler compiler;
        REQUIRE(compiler.parse("int main(void) { return 0; }"));
        auto* function = std::get_if<FunctionDecl>(
            &compiler.ast().decl(compiler.ast().translation_unit().decls[0]).node);
        REQUIRE(function != nullptr);
        CHECK(std::holds_alternative<TypeSpec::Int>(compiler.ast().type_spec(function->spec).node));
    }

    {
        TestCompiler compiler;
        REQUIRE(compiler.parse("struct S { int x; };"));
        auto* tag = std::get_if<TagDecl>(
            &compiler.ast().decl(compiler.ast().translation_unit().decls[0]).node);
        REQUIRE(tag != nullptr);
        const TypeSpec& spec = compiler.ast().type_spec(tag->spec);
        auto* structure = std::get_if<TypeSpec::StructDefinition>(&spec.node);
        REQUIRE(structure != nullptr);
        REQUIRE(structure->tag != nullptr);
        CHECK(*structure->tag == "S");
        CHECK(structure->fields.size() == 1);
    }

    {
        TestCompiler compiler;
        REQUIRE(compiler.parse("struct S;"));
        auto* tag = std::get_if<TagDecl>(
            &compiler.ast().decl(compiler.ast().translation_unit().decls[0]).node);
        REQUIRE(tag != nullptr);
        const TypeSpec& spec = compiler.ast().type_spec(tag->spec);
        auto* structure = std::get_if<TypeSpec::StructReference>(&spec.node);
        REQUIRE(structure != nullptr);
        REQUIRE(structure->tag != nullptr);
        CHECK(*structure->tag == "S");
    }

    {
        TestCompiler compiler;
        REQUIRE(compiler.parse("struct { int x; } s;"));
        auto* variable = std::get_if<VarDecl>(
            &compiler.ast().decl(compiler.ast().translation_unit().decls[0]).node);
        REQUIRE(variable != nullptr);
        const TypeSpec& spec = compiler.ast().type_spec(variable->spec);
        auto* structure = std::get_if<TypeSpec::StructDefinition>(&spec.node);
        REQUIRE(structure != nullptr);
        CHECK(structure->tag == nullptr);
        CHECK(structure->fields.size() == 1);
    }
}

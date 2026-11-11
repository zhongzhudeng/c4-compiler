#include "AST.hpp"

#include <type_traits>

DeclaratorShape declarator_shape(const AST& ast, DeclaratorId id, DeclaratorShape base) {
    if (!id.valid())
        return base;
    const Declarator& d = ast.declarator(id);
    if (std::holds_alternative<NameDeclarator>(d.node))
        return base;
    if (std::holds_alternative<PointerDeclarator>(d.node)) {
        const auto& p = std::get<PointerDeclarator>(d.node);
        return declarator_shape(ast, p.inner, DeclaratorShape::Pointer);
    }
    const auto& f = std::get<FunctionDeclarator>(d.node);
    return declarator_shape(ast, f.inner, DeclaratorShape::Function);
}

bool declarator_has_name(const AST& ast, DeclaratorId id) {
    if (!id.valid())
        return false;
    const Declarator& d = ast.declarator(id);
    if (std::holds_alternative<NameDeclarator>(d.node))
        return true;
    if (std::holds_alternative<PointerDeclarator>(d.node))
        return declarator_has_name(ast, std::get<PointerDeclarator>(d.node).inner);
    return declarator_has_name(ast, std::get<FunctionDeclarator>(d.node).inner);
}

Symbol declarator_name(const AST& ast, DeclaratorId id) {
    if (!id.valid())
        return Symbol();
    const Declarator& d = ast.declarator(id);
    if (std::holds_alternative<NameDeclarator>(d.node))
        return std::get<NameDeclarator>(d.node).name;
    if (std::holds_alternative<PointerDeclarator>(d.node))
        return declarator_name(ast, std::get<PointerDeclarator>(d.node).inner);
    return declarator_name(ast, std::get<FunctionDeclarator>(d.node).inner);
}

SourceLoc declarator_name_loc(const AST& ast, DeclaratorId id) {
    if (!id.valid())
        return SourceLoc{};
    const Declarator& d = ast.declarator(id);
    if (std::holds_alternative<NameDeclarator>(d.node))
        return d.loc;
    if (std::holds_alternative<PointerDeclarator>(d.node))
        return declarator_name_loc(ast, std::get<PointerDeclarator>(d.node).inner);
    return declarator_name_loc(ast, std::get<FunctionDeclarator>(d.node).inner);
}

bool declarator_produces_function(const AST& ast, DeclaratorId id) {
    return declarator_shape(ast, id) == DeclaratorShape::Function;
}

bool is_struct_type_spec(const TypeSpec& spec) {
    return std::holds_alternative<TypeSpec::StructReference>(spec.node) ||
           std::holds_alternative<TypeSpec::StructDefinition>(spec.node);
}

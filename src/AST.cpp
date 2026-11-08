#include "AST.hpp"

#include <type_traits>

DeclaratorShape declaratorShape(const AST &ast, DeclaratorId id,
                                DeclaratorShape base) {
  if (!id.valid())
    return base;
  const Declarator &d = ast.declarator(id);
  if (std::holds_alternative<NameDeclarator>(d.node))
    return base;
  if (std::holds_alternative<PointerDeclarator>(d.node)) {
    const auto &p = std::get<PointerDeclarator>(d.node);
    return declaratorShape(ast, p.inner, DeclaratorShape::Pointer);
  }
  const auto &f = std::get<FunctionDeclarator>(d.node);
  return declaratorShape(ast, f.inner, DeclaratorShape::Function);
}

bool declaratorHasName(const AST &ast, DeclaratorId id) {
  if (!id.valid())
    return false;
  const Declarator &d = ast.declarator(id);
  if (std::holds_alternative<NameDeclarator>(d.node))
    return true;
  if (std::holds_alternative<PointerDeclarator>(d.node))
    return declaratorHasName(ast, std::get<PointerDeclarator>(d.node).inner);
  return declaratorHasName(ast, std::get<FunctionDeclarator>(d.node).inner);
}

Symbol declaratorName(const AST &ast, DeclaratorId id) {
  if (!id.valid())
    return nullptr;
  const Declarator &d = ast.declarator(id);
  if (std::holds_alternative<NameDeclarator>(d.node))
    return std::get<NameDeclarator>(d.node).name;
  if (std::holds_alternative<PointerDeclarator>(d.node))
    return declaratorName(ast, std::get<PointerDeclarator>(d.node).inner);
  return declaratorName(ast, std::get<FunctionDeclarator>(d.node).inner);
}

SourceLoc declaratorNameLoc(const AST &ast, DeclaratorId id) {
  if (!id.valid())
    return SourceLoc{};
  const Declarator &d = ast.declarator(id);
  if (std::holds_alternative<NameDeclarator>(d.node))
    return d.loc;
  if (std::holds_alternative<PointerDeclarator>(d.node))
    return declaratorNameLoc(ast, std::get<PointerDeclarator>(d.node).inner);
  return declaratorNameLoc(ast, std::get<FunctionDeclarator>(d.node).inner);
}

bool declaratorProducesFunction(const AST &ast, DeclaratorId id) {
  return declaratorShape(ast, id) == DeclaratorShape::Function;
}

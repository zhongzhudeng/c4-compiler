#include "SemanticModel.hpp"

SemanticModel::SemanticModel(const AST &ast)
    : exprs_(ast.exprCount()), decls_(ast.declCount()),
      typeSpecs_(ast.typeSpecCount()), labels_(ast.labelCount()),
      gotoTargets_(ast.stmtCount()) {
  fileScope_ = addScope(ScopeKind::File, ScopeId{});
}

EntityId SemanticModel::addEntity(EntityKind kind, Symbol name,
                                  const Type *type, Linkage linkage,
                                  SourceLoc loc) {
  Entity entity;
  entity.kind = kind;
  entity.name = name;
  entity.type = type;
  entity.linkage = linkage;
  entity.loc = loc;
  entities_.push_back(std::move(entity));
  return EntityId(static_cast<uint32_t>(entities_.size() - 1));
}

ScopeId SemanticModel::addScope(ScopeKind kind, ScopeId parent) {
  Scope scope;
  scope.kind = kind;
  scope.parent = parent;
  scopes_.push_back(std::move(scope));
  return ScopeId(static_cast<uint32_t>(scopes_.size() - 1));
}

EntityId SemanticModel::binding(ScopeId scopeId, Symbol name) const {
  const Scope &current = scopes_.at(scopeId.index);
  auto it = current.ordinary.find(name);
  return it == current.ordinary.end() ? EntityId{} : it->second;
}

EntityId SemanticModel::lookup(ScopeId scopeId, Symbol name) const {
  for (ScopeId id = scopeId; id.valid(); id = scopes_.at(id.index).parent) {
    EntityId entity = binding(id, name);
    if (entity.valid())
      return entity;
  }
  return EntityId{};
}

bool SemanticModel::bind(ScopeId scopeId, Symbol name, EntityId entity) {
  return scopes_.at(scopeId.index).ordinary.emplace(name, entity).second;
}

StructDecl *SemanticModel::bindingTag(ScopeId scopeId, Symbol name) const {
  const Scope &current = scopes_.at(scopeId.index);
  auto it = current.tags.find(name);
  return it == current.tags.end() ? nullptr : it->second;
}

StructDecl *SemanticModel::lookupTag(ScopeId scopeId, Symbol name) const {
  for (ScopeId id = scopeId; id.valid(); id = scopes_.at(id.index).parent) {
    if (StructDecl *record = bindingTag(id, name))
      return record;
  }
  return nullptr;
}

bool SemanticModel::bindTag(ScopeId scopeId, Symbol name, StructDecl *record) {
  return scopes_.at(scopeId.index).tags.emplace(name, record).second;
}

LabelId SemanticModel::lookupLabel(ScopeId scopeId, Symbol name) const {
  // Labels have function scope, not lexical parent scope: only the function
  // scope that owns them is searched.
  const Scope &scope = scopes_.at(scopeId.index);
  auto it = scope.labels.find(name);
  return it == scope.labels.end() ? LabelId{} : it->second;
}

bool SemanticModel::bindLabel(ScopeId scopeId, Symbol name, LabelId label) {
  return scopes_.at(scopeId.index).labels.emplace(name, label).second;
}

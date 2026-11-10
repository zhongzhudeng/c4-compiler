#include "SemanticModel.hpp"

SemanticModel::SemanticModel(const AST& ast)
    : exprs_(ast.expr_count()), decls_(ast.decl_count()), type_specs_(ast.type_spec_count()),
      labels_(ast.label_count()), goto_targets_(ast.stmt_count()) {
    file_scope_ = add_scope(ScopeKind::File, ScopeId{});
}

EntityId SemanticModel::add_entity(EntityKind kind, Symbol name, const Type* type, Linkage linkage,
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

ScopeId SemanticModel::add_scope(ScopeKind kind, ScopeId parent) {
    Scope scope;
    scope.kind = kind;
    scope.parent = parent;
    scopes_.push_back(std::move(scope));
    return ScopeId(static_cast<uint32_t>(scopes_.size() - 1));
}

EntityId SemanticModel::binding(ScopeId scope_id, Symbol name) const {
    const Scope& current = scopes_.at(scope_id.index);
    auto it = current.ordinary.find(name);
    return it == current.ordinary.end() ? EntityId{} : it->second;
}

EntityId SemanticModel::lookup(ScopeId scope_id, Symbol name) const {
    for (ScopeId id = scope_id; id.valid(); id = scopes_.at(id.index).parent) {
        EntityId entity = binding(id, name);
        if (entity.valid())
            return entity;
    }
    return EntityId{};
}

bool SemanticModel::bind(ScopeId scope_id, Symbol name, EntityId entity) {
    return scopes_.at(scope_id.index).ordinary.emplace(name, entity).second;
}

StructDecl* SemanticModel::binding_tag(ScopeId scope_id, Symbol name) const {
    const Scope& current = scopes_.at(scope_id.index);
    auto it = current.tags.find(name);
    return it == current.tags.end() ? nullptr : it->second;
}

StructDecl* SemanticModel::lookup_tag(ScopeId scope_id, Symbol name) const {
    for (ScopeId id = scope_id; id.valid(); id = scopes_.at(id.index).parent) {
        if (StructDecl* record = binding_tag(id, name))
            return record;
    }
    return nullptr;
}

bool SemanticModel::bind_tag(ScopeId scope_id, Symbol name, StructDecl* record) {
    return scopes_.at(scope_id.index).tags.emplace(name, record).second;
}

LabelId SemanticModel::lookup_label(ScopeId scope_id, Symbol name) const {
    // Labels have function scope, not lexical parent scope: only the function
    // scope that owns them is searched.
    const Scope& scope = scopes_.at(scope_id.index);
    auto it = scope.labels.find(name);
    return it == scope.labels.end() ? LabelId{} : it->second;
}

bool SemanticModel::bind_label(ScopeId scope_id, Symbol name, LabelId label) {
    return scopes_.at(scope_id.index).labels.emplace(name, label).second;
}

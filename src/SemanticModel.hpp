#pragma once

#include "AST.hpp"
#include "Support.hpp"
#include "Type.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

/// Value categories are semantic, not syntactic.
enum class ValueCategory { LValue, RValue, FunctionDesignator, Void };

/// Semantic identities.  A DeclId names syntax (AST.hpp); an EntityId names a
/// declared object or function and is shared by every declaration that denotes
/// it.  A ScopeId names a lexical scope and stays stable for the whole
/// translation unit.
struct EntityTag {};
struct ScopeTag {};
using EntityId = NodeId<EntityTag>;
using ScopeId = NodeId<ScopeTag>;

enum class EntityKind { Object, Function };

/// C4 has no storage-class specifiers: file-scope objects and all functions
/// have external linkage; block-scope objects and parameters have none.
enum class Linkage { None, External };

enum class DefinitionState {
    Declared,  // only declarations seen so far
    Tentative, // file-scope object declaration (C4 has no initializers)
    Defined,   // function body or block-scope object/parameter
};

enum class ScopeKind {
    File,
    Block,
    FunctionPrototype, // parameter list of a non-defining declarator
    Function,          // definition parameters + body, and labels
};

struct FieldInfo {
    Symbol name;
    const Type* type = nullptr;
    DeclId decl;
    uint64_t offset = 0;
};

/// A struct/union tag record.  This is a semantic entity, not an AST node.
/// Struct types point at it, which is what permits recursive and incomplete
/// types.  Fields are populated during Sema.
struct StructDecl {
    Symbol tag;
    SourceLoc loc;
    std::vector<FieldInfo> fields;
    bool complete = false;
    bool anonymous = false;
    const Type* type = nullptr;
};

/// A declared object or function.  The entity, not any declaration, owns the
/// canonical type, linkage and definition state; declarations only point at it.
/// Every declaration that denotes the entity is recorded in declarations.
struct Entity {
    Symbol name;
    const Type* type = nullptr;
    EntityKind kind = EntityKind::Object;
    Linkage linkage = Linkage::None;
    DefinitionState definition = DefinitionState::Declared;
    SourceLoc loc; // first declaration, for diagnostics
    std::vector<DeclId> declarations;
    std::vector<DeclId> parameters; // parameter declarations of the definition
};

/// A lexical scope.  Bindings are namespace-specific: ordinary identifiers
/// resolve to entities, tags to StructDecl records, and labels (function scope
/// only) to label declarations.  Members are not lexical bindings; they live
/// in StructDecl::fields.
struct Scope {
    ScopeKind kind = ScopeKind::Block;
    ScopeId parent;
    std::unordered_map<Symbol, EntityId> ordinary;
    std::unordered_map<Symbol, StructDecl*> tags;
    std::unordered_map<Symbol, LabelId> labels;
};

/// The value of an integer constant expression.  null_pointer_constant is set
/// only for the literal token 0: a parenthesized or folded zero is an ordinary
/// integer constant (S-009).
struct ConstantValue {
    int32_t value = 0;
    bool null_pointer_constant = false;
};

/// A resolved member access: the record and the index of the selected field.
struct MemberRef {
    StructDecl* record = nullptr;
    unsigned index = 0;
};

/// The decoded bytes of a string literal, without the terminating NUL.
struct StringLiteralData {
    std::vector<char> bytes;
};

/// Expression-specific semantic information.  An expression kind carries at
/// most one of these: a constant has a value, an identifier names an entity, a
/// member access names a field, and a string literal carries its decoded
/// bytes.  Operators, calls and sizeof need no extra semantics.
using ExprDetails =
    std::variant<std::monostate, ConstantValue, EntityId, MemberRef, StringLiteralData>;

struct ExprInfo {
    const Type* type = nullptr;
    ValueCategory category = ValueCategory::RValue;
    ExprDetails details;

    const ConstantValue* constant() const {
        return std::get_if<ConstantValue>(&details);
    }
    bool is_constant() const {
        return constant() != nullptr;
    }
    bool is_null_pointer_constant() const {
        const ConstantValue* value = constant();
        return value && value->null_pointer_constant;
    }
    const EntityId* entity() const {
        return std::get_if<EntityId>(&details);
    }
    const MemberRef* member() const {
        return std::get_if<MemberRef>(&details);
    }
    const StringLiteralData* string_literal() const {
        return std::get_if<StringLiteralData>(&details);
    }
};

/// A declaration's semantic links.  Type, name, linkage and definition state
/// live on the entity; the scope records where this declaration's binding was
/// introduced.  Tag declarations point at their StructDecl; fields and
/// unnamed parameters have no ordinary binding.
struct DeclInfo {
    EntityId entity;
    ScopeId scope;
    StructDecl* record = nullptr; // TagDecl
};

struct TypeSpecInfo {
    StructDecl* record = nullptr;
};

struct LabelInfo {
    bool defined = false;
    SourceLoc loc;
};

/// Semantic results keyed by AST node ID.  The model never owns AST nodes and
/// AST nodes never contain semantic results.  It is sized once from the
/// completed AST; Sema fills it in.  The model also owns the translation unit's
/// entities and lexical scopes: EntityIds and ScopeIds are indices into stable
/// storage, so they never depend on the position of a mutable scope stack.
class SemanticModel {
public:
    explicit SemanticModel(const AST& ast);

    ExprInfo& expr(ExprId id) {
        return exprs_.at(id.index);
    }
    const ExprInfo& expr(ExprId id) const {
        return exprs_.at(id.index);
    }

    DeclInfo& decl(DeclId id) {
        return decls_.at(id.index);
    }
    const DeclInfo& decl(DeclId id) const {
        return decls_.at(id.index);
    }

    TypeSpecInfo& type_spec(TypeSpecId id) {
        return type_specs_.at(id.index);
    }
    const TypeSpecInfo& type_spec(TypeSpecId id) const {
        return type_specs_.at(id.index);
    }

    LabelInfo& label(LabelId id) {
        return labels_.at(id.index);
    }
    const LabelInfo& label(LabelId id) const {
        return labels_.at(id.index);
    }

    LabelId& goto_target(StmtId id) {
        return goto_targets_.at(id.index);
    }
    const LabelId& goto_target(StmtId id) const {
        return goto_targets_.at(id.index);
    }

    EntityId add_entity(EntityKind kind, Symbol name, const Type* type, Linkage linkage,
                        SourceLoc loc);
    Entity& entity(EntityId id) {
        return entities_.at(id.index);
    }
    const Entity& entity(EntityId id) const {
        return entities_.at(id.index);
    }
    size_t entity_count() const {
        return entities_.size();
    }

    ScopeId file_scope() const {
        return file_scope_;
    }
    ScopeId add_scope(ScopeKind kind, ScopeId parent);
    Scope& scope(ScopeId id) {
        return scopes_.at(id.index);
    }
    const Scope& scope(ScopeId id) const {
        return scopes_.at(id.index);
    }
    size_t scope_count() const {
        return scopes_.size();
    }

    EntityId binding(ScopeId scope, Symbol name) const;
    EntityId lookup(ScopeId scope, Symbol name) const;
    bool bind(ScopeId scope, Symbol name, EntityId entity);

    StructDecl* binding_tag(ScopeId scope, Symbol name) const;
    StructDecl* lookup_tag(ScopeId scope, Symbol name) const;
    bool bind_tag(ScopeId scope, Symbol name, StructDecl* record);

    LabelId lookup_label(ScopeId scope, Symbol name) const;
    bool bind_label(ScopeId scope, Symbol name, LabelId label);

    StructDecl* add_struct(Symbol tag, SourceLoc loc, bool anonymous) {
        auto owned = std::make_unique<StructDecl>();
        owned->tag = tag;
        owned->loc = loc;
        owned->anonymous = anonymous;
        StructDecl* result = owned.get();
        structs_.push_back(std::move(owned));
        return result;
    }

private:
    std::vector<ExprInfo> exprs_;
    std::vector<DeclInfo> decls_;
    std::vector<TypeSpecInfo> type_specs_;
    std::vector<LabelInfo> labels_;
    std::vector<LabelId> goto_targets_;
    std::vector<std::unique_ptr<StructDecl>> structs_;

    // Stable storage: push_back does not invalidate references, and the indices
    // are the EntityId/ScopeId values.
    std::deque<Entity> entities_;
    std::deque<Scope> scopes_;
    ScopeId file_scope_;
};

#include "Sema.hpp"

#include <cassert>
#include <climits>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

namespace {

int32_t wrap_int(long long value) {
    return static_cast<int32_t>(static_cast<uint32_t>(value));
}

int escape_value(char c, SourceLoc loc, Diagnostics& diagnostics) {
    switch (c) {
        case '\'':
            return '\'';
        case '"':
            return '"';
        case '?':
            return '?';
        case '\\':
            return '\\';
        case 'a':
            return 7;
        case 'b':
            return 8;
        case 'f':
            return 12;
        case 'n':
            return 10;
        case 'r':
            return 13;
        case 't':
            return 9;
        case 'v':
            return 11;
        default:
            diagnostics.error(loc, "invalid escape sequence");
    }
}

long long decode_character_constant(Symbol spelling, SourceLoc loc, Diagnostics& diagnostics) {
    const std::string& text = *spelling;
    assert(text.size() >= 3 && text.front() == '\'' && text.back() == '\'');
    if (text.size() == 3) {
        // The execution character set is ISO-8859-1 and char is signed.
        unsigned char byte = static_cast<unsigned char>(text[1]);
        return static_cast<signed char>(byte);
    }
    assert(text.size() == 4 && text[1] == '\\');
    return escape_value(text[2], loc, diagnostics);
}

long long decode_integer_constant(Symbol spelling, SourceLoc loc, Diagnostics& diagnostics) {
    const std::string& text = *spelling;
    if (!text.empty() && text.front() == '\'')
        return decode_character_constant(spelling, loc, diagnostics);

    long long value = 0;
    for (char c : text) {
        if (c < '0' || c > '9')
            diagnostics.error(loc, "invalid integer constant");
        value = value * 10 + (c - '0');
        if (value > INT_MAX)
            diagnostics.error(loc, "integer constant is too large");
    }
    return value;
}

std::vector<char> decode_string_literal(Symbol spelling, SourceLoc loc, Diagnostics& diagnostics) {
    const std::string& text = *spelling;
    assert(text.size() >= 2 && text.front() == '"' && text.back() == '"');
    std::vector<char> bytes;
    bytes.reserve(text.size() - 2);
    for (size_t i = 1; i + 1 < text.size(); ++i) {
        char c = text[i];
        if (c == '\\') {
            ++i;
            if (i + 1 > text.size())
                diagnostics.error(loc, "invalid escape sequence");
            bytes.push_back(static_cast<char>(escape_value(text[i], loc, diagnostics)));
        } else {
            bytes.push_back(c);
        }
    }
    return bytes;
}

SourceLoc declaration_name_loc(const AST& ast, DeclId id) {
    const Decl& decl = ast.decl(id);
    DeclaratorId declarator;
    if (auto* var = std::get_if<VarDecl>(&decl.node))
        declarator = var->declarator;
    else if (auto* fun = std::get_if<FunctionDecl>(&decl.node))
        declarator = fun->declarator;
    else if (auto* field = std::get_if<FieldDecl>(&decl.node))
        declarator = field->declarator;
    else if (auto* param = std::get_if<ParamDecl>(&decl.node))
        declarator = param->declarator;
    else
        return decl.loc;
    SourceLoc loc = declarator_name_loc(ast, declarator);
    return loc.line ? loc : decl.loc;
}

} // namespace

Sema::Sema(const AST& ast, SemanticModel& model, TypeContext& types, TypeLayout& layout,
           Diagnostics& diagnostics)
    : ast_(ast), model_(model), types_(types), layout_(layout), diagnostics_(diagnostics) {
    current_ = model_.file_scope();
}

void Sema::analyze() {
    for (DeclId id : ast_.translation_unit().decls)
        analyze_top_level(id);
    // File-scope tentative definitions may name an incomplete struct that is
    // completed later in the translation unit, so check them only now.
    for (size_t i = 0; i < model_.entity_count(); ++i) {
        EntityId id{static_cast<uint32_t>(i)};
        const Entity& entity = model_.entity(id);
        if (entity.kind == EntityKind::Object && entity.definition == DefinitionState::Tentative)
            validate_object_type(entity.type, entity.loc, "variable");
    }
}

ScopeId Sema::push_scope(ScopeKind kind) {
    ScopeId scope = model_.add_scope(kind, current_);
    current_ = scope;
    return scope;
}

void Sema::pop_scope() {
    ScopeId parent = model_.scope(current_).parent;
    assert(parent.valid() && "cannot pop the file scope");
    current_ = parent;
}

EntityId Sema::lookup_ordinary(Symbol name) const {
    return model_.lookup(current_, name);
}

StructDecl* Sema::lookup_tag(Symbol name) const {
    return model_.lookup_tag(current_, name);
}

void Sema::record_declaration(DeclId id, EntityId entity, ScopeId scope) {
    model_.entity(entity).declarations.push_back(id);
    DeclInfo& info = model_.decl(id);
    info.entity = entity;
    info.scope = scope;
}

EntityId Sema::declare_file_object(DeclId id, const DeclResult& result, SourceLoc loc) {
    ScopeId file = model_.file_scope();
    EntityId entity = model_.binding(file, result.name);

    if (entity.valid()) {
        const Entity& existing = model_.entity(entity);
        if (existing.kind != EntityKind::Object)
            diagnostics_.error(loc, "redefinition of '" + *result.name +
                                        "' as a different kind of symbol");
        if (existing.type != result.type)
            diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
    } else {
        auto external = external_entities_.find(result.name);
        if (external != external_entities_.end()) {
            // A block-scope function declaration created the external entity before
            // any file-scope declaration made the name visible there.
            entity = external->second;
            const Entity& existing = model_.entity(entity);
            if (existing.kind != EntityKind::Object)
                diagnostics_.error(loc, "redefinition of '" + *result.name +
                                            "' as a different kind of symbol");
            if (existing.type != result.type)
                diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
            model_.bind(file, result.name, entity);
        } else {
            entity = model_.add_entity(EntityKind::Object, result.name, result.type,
                                       Linkage::External, loc);
            external_entities_.emplace(result.name, entity);
            model_.bind(file, result.name, entity);
        }
    }

    model_.entity(entity).definition = DefinitionState::Tentative;
    record_declaration(id, entity, file);
    return entity;
}

EntityId Sema::declare_file_function(DeclId id, const DeclResult& result, SourceLoc loc,
                                     bool is_definition) {
    ScopeId file = model_.file_scope();
    EntityId entity = model_.binding(file, result.name);

    if (entity.valid()) {
        const Entity& existing = model_.entity(entity);
        if (existing.kind != EntityKind::Function)
            diagnostics_.error(loc, "redefinition of '" + *result.name +
                                        "' as a different kind of symbol");
        if (existing.type != result.type)
            diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
    } else {
        auto external = external_entities_.find(result.name);
        if (external != external_entities_.end()) {
            entity = external->second;
            const Entity& existing = model_.entity(entity);
            if (existing.kind != EntityKind::Function)
                diagnostics_.error(loc, "redefinition of '" + *result.name +
                                            "' as a different kind of symbol");
            if (existing.type != result.type)
                diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
            model_.bind(file, result.name, entity);
        } else {
            entity = model_.add_entity(EntityKind::Function, result.name, result.type,
                                       Linkage::External, loc);
            external_entities_.emplace(result.name, entity);
            model_.bind(file, result.name, entity);
        }
    }

    Entity& declared = model_.entity(entity);
    if (is_definition) {
        if (declared.definition == DefinitionState::Defined)
            diagnostics_.error(loc, "redefinition of function '" + *result.name + "'");
        declared.definition = DefinitionState::Defined;
        declared.parameters = result.params;
    }
    record_declaration(id, entity, file);
    return entity;
}

EntityId Sema::declare_local_object(DeclId id, const DeclResult& result, SourceLoc loc) {
    if (model_.binding(current_, result.name).valid())
        diagnostics_.error(loc, "redefinition of '" + *result.name + "'");
    EntityId entity =
        model_.add_entity(EntityKind::Object, result.name, result.type, Linkage::None, loc);
    model_.entity(entity).definition = DefinitionState::Defined;
    model_.bind(current_, result.name, entity);
    record_declaration(id, entity, current_);
    return entity;
}

EntityId Sema::declare_local_function(DeclId id, const DeclResult& result, SourceLoc loc) {
    EntityId bound = model_.binding(current_, result.name);
    if (bound.valid()) {
        const Entity& existing = model_.entity(bound);
        if (existing.kind != EntityKind::Function)
            diagnostics_.error(loc, "redefinition of '" + *result.name +
                                        "' as a different kind of symbol");
        if (existing.type != result.type)
            diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
        record_declaration(id, bound, current_);
        return bound;
    }

    EntityId visible = lookup_ordinary(result.name);
    if (visible.valid()) {
        const Entity& existing = model_.entity(visible);
        if (existing.kind == EntityKind::Function) {
            if (existing.type != result.type)
                diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
            model_.bind(current_, result.name, visible);
            record_declaration(id, visible, current_);
            return visible;
        }
        // An object in an enclosing block is hidden by the function declaration;
        // the declaration denotes the external function instead.
    }

    auto external = external_entities_.find(result.name);
    if (external != external_entities_.end()) {
        EntityId entity = external->second;
        const Entity& existing = model_.entity(entity);
        if (existing.kind != EntityKind::Function)
            diagnostics_.error(loc, "redefinition of '" + *result.name +
                                        "' as a different kind of symbol");
        if (existing.type != result.type)
            diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
        model_.bind(current_, result.name, entity);
        record_declaration(id, entity, current_);
        return entity;
    }

    EntityId entity =
        model_.add_entity(EntityKind::Function, result.name, result.type, Linkage::External, loc);
    external_entities_.emplace(result.name, entity);
    model_.bind(current_, result.name, entity);
    record_declaration(id, entity, current_);
    return entity;
}

const Type* Sema::resolve_type_spec(TypeSpecId id, bool declare_tag) {
    const TypeSpec& spec = ast_.type_spec(id);
    if (std::holds_alternative<TypeSpec::Void>(spec.node))
        return types_.void_type();
    if (std::holds_alternative<TypeSpec::Char>(spec.node))
        return types_.char_type();
    if (std::holds_alternative<TypeSpec::Int>(spec.node))
        return types_.int_type();

    const TypeSpec::Struct& struct_spec = std::get<TypeSpec::Struct>(spec.node);
    StructDecl* record = model_.type_spec(id).record;
    if (record)
        return types_.struct_type(record);

    if (struct_spec.has_braces) {
        if (struct_spec.tag) {
            record = model_.binding_tag(current_, struct_spec.tag);
            if (record) {
                if (record->complete)
                    diagnostics_.error(spec.loc,
                                       "redefinition of 'struct " + *struct_spec.tag + "'");
            } else {
                record = model_.add_struct(struct_spec.tag, spec.loc, false);
                model_.bind_tag(current_, struct_spec.tag, record);
            }
        } else {
            record = model_.add_struct(Symbol(), spec.loc, true);
        }

        record->loc = spec.loc;
        record->anonymous = !struct_spec.tag;
        record->type = types_.struct_type(record);
        record->fields.clear();
        model_.type_spec(id).record = record;

        std::unordered_set<Symbol> field_names;
        for (DeclId field_id : struct_spec.fields) {
            const Decl& field = ast_.decl(field_id);
            auto* field_decl = std::get_if<FieldDecl>(&field.node);
            if (!field_decl)
                diagnostics_.error(field.loc, "invalid struct member");

            const Type* base = resolve_type_spec(field_decl->spec);
            DeclResult result = resolve_declarator(field_decl->declarator, base);
            SourceLoc name_loc = result.name_loc.line ? result.name_loc : field.loc;
            if (!result.name)
                diagnostics_.error(name_loc, "struct member requires a name");
            if (!field_names.insert(result.name).second)
                diagnostics_.error(name_loc, "duplicate struct member '" + *result.name + "'");
            validate_field_type(result.type, result.name, name_loc);

            FieldInfo info;
            info.name = result.name;
            info.type = result.type;
            info.decl = field_id;
            record->fields.push_back(std::move(info));
        }

        if (record->fields.empty())
            diagnostics_.error(spec.loc, "struct has no members");

        record->complete = true;
        for (size_t i = 0; i < record->fields.size(); ++i)
            record->fields[i].offset = layout_.field_offset(record, unsigned(i));
        return record->type;
    }

    if (!struct_spec.tag)
        diagnostics_.error(spec.loc, "expected struct tag or declaration");

    if (declare_tag) {
        // A declaration of the form "struct S;" declares the tag in the current
        // scope even when an outer scope already declares a tag with that name.
        record = model_.binding_tag(current_, struct_spec.tag);
        if (!record) {
            record = model_.add_struct(struct_spec.tag, spec.loc, false);
            record->type = types_.struct_type(record);
            model_.bind_tag(current_, struct_spec.tag, record);
        }
    } else {
        record = lookup_tag(struct_spec.tag);
        if (!record) {
            record = model_.add_struct(struct_spec.tag, spec.loc, false);
            record->type = types_.struct_type(record);
            model_.bind_tag(current_, struct_spec.tag, record);
        }
    }
    model_.type_spec(id).record = record;
    return types_.struct_type(record);
}

Sema::DeclResult Sema::resolve_declarator(DeclaratorId id, const Type* base) {
    DeclResult result;
    result.type = base;
    if (!id.valid())
        return result;

    const Declarator& declarator = ast_.declarator(id);
    if (auto* name = std::get_if<NameDeclarator>(&declarator.node)) {
        result.name = name->name;
        result.name_loc = declarator.loc;
    } else if (auto* pointer = std::get_if<PointerDeclarator>(&declarator.node)) {
        result = resolve_declarator(pointer->inner, types_.pointer_to(base));
    } else if (auto* function = std::get_if<FunctionDeclarator>(&declarator.node)) {
        // C11 6.2.1p4/p7: names and tags declared in a parameter list have
        // function prototype scope, which ends with the function declarator.  The
        // scope is retained only when this declarator produces the declared
        // function type and the declaration is a definition (adoptFunctionScope).
        ScopeId prototype_scope = push_scope(ScopeKind::FunctionPrototype);
        std::vector<const Type*> parameter_types;
        std::vector<DeclId> parameters = function->params;
        std::unordered_set<Symbol> parameter_names;

        for (DeclId param_id : parameters) {
            const Decl& param = ast_.decl(param_id);
            auto* param_decl = std::get_if<ParamDecl>(&param.node);
            if (!param_decl)
                diagnostics_.error(param.loc, "invalid parameter declaration");

            const Type* param_base = resolve_type_spec(param_decl->spec);
            DeclResult param_result = resolve_declarator(param_decl->declarator, param_base);
            const Type* param_type = adjust_parameter_type(param_result.type, param.loc);

            if (Type::is_void(param_type))
                diagnostics_.error(param.loc, "parameter has incomplete type 'void'");
            if (Type::is_function(param_type))
                diagnostics_.error(param.loc, "parameter has function type");
            SourceLoc param_loc = param_result.name_loc.line ? param_result.name_loc : param.loc;
            if (param_result.name) {
                if (!parameter_names.insert(param_result.name).second)
                    diagnostics_.error(param_loc,
                                       "redefinition of parameter '" + *param_result.name + "'");
                EntityId entity = model_.add_entity(EntityKind::Object, param_result.name,
                                                    param_type, Linkage::None, param_loc);
                model_.entity(entity).definition = DefinitionState::Defined;
                model_.bind(prototype_scope, param_result.name, entity);
                record_declaration(param_id, entity, prototype_scope);
            }
            parameter_types.push_back(param_type);
        }

        const Type* function_type = types_.function_of(base, std::move(parameter_types));
        validate_function_type(function_type, declarator.loc);
        DeclResult inner = resolve_declarator(function->inner, function_type);
        if (inner.type == function_type) {
            inner.function_scope = prototype_scope;
            inner.params = std::move(parameters);
        }
        pop_scope();
        result = std::move(inner);
    }

    return result;
}

void Sema::adopt_function_scope(ScopeId scope, ScopeId parent) {
    assert(scope.valid() && "function definition has no function scope");
    Scope& function_scope = model_.scope(scope);
    function_scope.kind = ScopeKind::Function;
    function_scope.parent = parent;
}

const Type* Sema::adjust_parameter_type(const Type* type, SourceLoc loc) {
    (void)loc;
    if (Type::is_array(type))
        return types_.pointer_to(type->element());
    if (Type::is_function(type))
        return types_.pointer_to(type);
    return type;
}

bool Sema::is_complete_object(const Type* type) const {
    if (!type || !Type::is_object(type))
        return false;
    if (Type::is_struct(type))
        return type->record() && type->record()->complete;
    if (Type::is_array(type))
        return is_complete_object(type->element());
    return true;
}

void Sema::validate_object_type(const Type* type, SourceLoc loc, const char* what,
                                bool require_complete) const {
    if (Type::is_void(type))
        diagnostics_.error(loc, std::string(what) + " has incomplete type 'void'");
    if (Type::is_function(type))
        diagnostics_.error(loc, std::string(what) + " declared as a function");
    if (require_complete && !is_complete_object(type))
        diagnostics_.error(loc, std::string(what) + " has incomplete type '" +
                                    TypeContext::to_string(type) + "'");
}

void Sema::validate_function_type(const Type* type, SourceLoc loc) const {
    assert(Type::is_function(type));
    const Type* result = type->result();
    if (Type::is_function(result) || Type::is_array(result))
        diagnostics_.error(loc, "function cannot return a function or array type");
    if (Type::is_struct(result)) {
        if (!result->record() || !result->record()->complete)
            diagnostics_.error(loc, "function returns an incomplete struct type");
        diagnostics_.error(loc, "functions returning struct are not supported");
    }
    for (const Type* param : type->params()) {
        if (Type::is_void(param))
            diagnostics_.error(loc, "parameter has incomplete type 'void'");
        if (Type::is_function(param) || Type::is_array(param))
            diagnostics_.error(loc, "parameter has an invalid type");
        if (Type::is_struct(param)) {
            if (!param->record() || !param->record()->complete)
                diagnostics_.error(loc, "parameter has incomplete struct type");
            diagnostics_.error(loc, "struct parameters are not supported");
        }
    }
}

void Sema::validate_field_type(const Type* type, Symbol name, SourceLoc loc) const {
    if (Type::is_void(type))
        diagnostics_.error(loc, "member has incomplete type 'void'");
    if (Type::is_function(type))
        diagnostics_.error(loc, "member declared as a function");
    if (!is_complete_object(type)) {
        std::string message = "member";
        if (name)
            message += " '" + *name + "'";
        message += " has incomplete type '" + TypeContext::to_string(type) + "'";
        diagnostics_.error(loc, message);
    }
}

void Sema::analyze_top_level(DeclId id) {
    const Decl& decl = ast_.decl(id);

    if (auto* var = std::get_if<VarDecl>(&decl.node)) {
        const Type* base = resolve_type_spec(var->spec);
        DeclResult result = resolve_declarator(var->declarator, base);
        SourceLoc name_loc = result.name_loc.line ? result.name_loc : decl.loc;
        if (!result.name)
            diagnostics_.error(name_loc, "declaration requires a name");
        // A file-scope object declaration without an initializer is a tentative
        // definition.  C11 permits an incomplete type here if the type is
        // completed later in the translation unit, so completeness is checked
        // after all top-level declarations have been analyzed.
        validate_object_type(result.type, name_loc, "variable",
                             /*requireComplete=*/false);
        declare_file_object(id, result, name_loc);
        return;
    }

    if (auto* function = std::get_if<FunctionDecl>(&decl.node)) {
        const Type* base = resolve_type_spec(function->spec);
        bool is_definition = function->body.valid();
        DeclResult result = resolve_declarator(function->declarator, base);
        SourceLoc name_loc = result.name_loc.line ? result.name_loc : decl.loc;
        if (!result.name)
            diagnostics_.error(name_loc, "function declaration requires a name");
        if (!Type::is_function(result.type))
            diagnostics_.error(name_loc, "function declaration does not declare a function");
        validate_function_type(result.type, name_loc);

        declare_file_function(id, result, name_loc, is_definition);
        if (is_definition)
            analyze_function(id, result.function_scope);
        return;
    }

    if (auto* tag = std::get_if<TagDecl>(&decl.node)) {
        const TypeSpec& spec = ast_.type_spec(tag->spec);
        const auto* struct_spec = std::get_if<TypeSpec::Struct>(&spec.node);
        if (struct_spec && struct_spec->has_braces && !struct_spec->tag) {
            diagnostics_.error(decl.loc, "declaration does not declare anything");
            return;
        }
        resolve_type_spec(tag->spec, /*declareTag=*/true);
        DeclInfo& info = model_.decl(id);
        info.record = model_.type_spec(tag->spec).record;
        info.scope = current_;
        return;
    }

    diagnostics_.error(decl.loc, "invalid top-level declaration");
}

void Sema::analyze_local_decl(DeclId id) {
    const Decl& decl = ast_.decl(id);

    if (auto* var = std::get_if<VarDecl>(&decl.node)) {
        const Type* base = resolve_type_spec(var->spec);
        DeclResult result = resolve_declarator(var->declarator, base);
        SourceLoc name_loc = result.name_loc.line ? result.name_loc : decl.loc;
        if (!result.name)
            diagnostics_.error(name_loc, "declaration requires a name");
        validate_object_type(result.type, name_loc, "variable");
        declare_local_object(id, result, name_loc);
        return;
    }

    if (auto* function = std::get_if<FunctionDecl>(&decl.node)) {
        const Type* base = resolve_type_spec(function->spec);
        DeclResult result = resolve_declarator(function->declarator, base);
        SourceLoc name_loc = result.name_loc.line ? result.name_loc : decl.loc;
        if (!result.name)
            diagnostics_.error(name_loc, "function declaration requires a name");
        if (!Type::is_function(result.type))
            diagnostics_.error(name_loc, "function declaration does not declare a function");
        validate_function_type(result.type, name_loc);
        declare_local_function(id, result, name_loc);
        return;
    }

    if (auto* tag = std::get_if<TagDecl>(&decl.node)) {
        const TypeSpec& spec = ast_.type_spec(tag->spec);
        const auto* struct_spec = std::get_if<TypeSpec::Struct>(&spec.node);
        if (struct_spec && struct_spec->has_braces && !struct_spec->tag) {
            diagnostics_.error(decl.loc, "declaration does not declare anything");
            return;
        }
        resolve_type_spec(tag->spec, /*declareTag=*/true);
        DeclInfo& info = model_.decl(id);
        info.record = model_.type_spec(tag->spec).record;
        info.scope = current_;
        return;
    }

    diagnostics_.error(decl.loc, "invalid local declaration");
}

void Sema::analyze_function(DeclId id, ScopeId function_scope) {
    auto* function = std::get_if<FunctionDecl>(&ast_.decl(id).node);
    assert(function && function->body.valid());
    EntityId entity_id = model_.decl(id).entity;
    const Entity& entity = model_.entity(entity_id);
    const Type* function_type = entity.type;
    assert(Type::is_function(function_type));

    for (DeclId param_id : entity.parameters) {
        if (!model_.decl(param_id).entity.valid())
            diagnostics_.error(declaration_name_loc(ast_, param_id),
                               "parameter name omitted in function definition");
    }

    // The prototype scope of the definition's function declarator becomes the
    // function scope: its parameters and parameter-list tags stay visible in the
    // body, and labels belong to it.  Its parent is the scope containing the
    // definition, not any enclosing declarator's prototype scope.
    adopt_function_scope(function_scope, current_);
    ScopeId saved = current_;
    current_ = function_scope;

    FunctionContext context;
    context.return_type = function_type->result();
    context.scope = function_scope;
    function_ = &context;

    analyze_compound(function->body, false);

    for (const auto& [goto_stmt, label_name] : context.pending_gotos) {
        LabelId target = model_.lookup_label(context.scope, label_name);
        if (!target.valid())
            diagnostics_.error(ast_.stmt(goto_stmt).loc,
                               "use of undeclared label '" + *label_name + "'");
        model_.goto_target(goto_stmt) = target;
    }

    function_ = nullptr;
    current_ = saved;
}

void Sema::analyze_compound(StmtId id, bool push) {
    auto* compound = std::get_if<CompoundStmt>(&ast_.stmt(id).node);
    assert(compound && "expected a compound statement");
    if (push)
        push_scope(ScopeKind::Block);
    for (StmtId item : compound->items) {
        if (auto* decl_stmt = std::get_if<DeclStmt>(&ast_.stmt(item).node))
            analyze_local_decl(decl_stmt->decl);
        else
            analyze_stmt(item);
    }
    if (push)
        pop_scope();
}

void Sema::analyze_stmt(StmtId id) {
    const Stmt& stmt = ast_.stmt(id);
    SourceLoc loc = stmt.loc;

    if (std::holds_alternative<CompoundStmt>(stmt.node)) {
        analyze_compound(id, true);
        return;
    }

    if (auto* decl_stmt = std::get_if<DeclStmt>(&stmt.node)) {
        analyze_local_decl(decl_stmt->decl);
        return;
    }

    if (auto* expr_stmt = std::get_if<ExprStmt>(&stmt.node)) {
        if (expr_stmt->expr.valid())
            check_expr(expr_stmt->expr);
        return;
    }

    if (auto* if_stmt = std::get_if<IfStmt>(&stmt.node)) {
        check_scalar(if_stmt->cond, loc, "'if' condition");
        analyze_stmt(if_stmt->then_stmt);
        if (if_stmt->else_stmt.valid())
            analyze_stmt(if_stmt->else_stmt);
        return;
    }

    if (auto* while_stmt = std::get_if<WhileStmt>(&stmt.node)) {
        check_scalar(while_stmt->cond, loc, "'while' condition");
        assert(function_ && "loop outside of a function");
        ++function_->loop_depth;
        analyze_stmt(while_stmt->body);
        --function_->loop_depth;
        return;
    }

    if (auto* return_stmt = std::get_if<ReturnStmt>(&stmt.node)) {
        assert(function_ && "return outside of a function");
        const Type* return_type = function_->return_type;
        if (return_stmt->expr.valid()) {
            check_expr(return_stmt->expr);
            if (Type::is_void(return_type))
                diagnostics_.error(loc, "return with a value in a void function");
            if (!assignable(return_type, return_stmt->expr))
                diagnostics_.error(
                    loc, "returning '" +
                             TypeContext::to_string(decayed_type(model_.expr(return_stmt->expr))) +
                             "' from a function with return type '" +
                             TypeContext::to_string(return_type) + "'");
        } else if (!Type::is_void(return_type)) {
            diagnostics_.error(loc, "return without a value in a non-void function");
        }
        return;
    }

    if (std::holds_alternative<BreakStmt>(stmt.node)) {
        if (!function_ || function_->loop_depth == 0)
            diagnostics_.error(loc, "'break' outside of a loop");
        return;
    }

    if (std::holds_alternative<ContinueStmt>(stmt.node)) {
        if (!function_ || function_->loop_depth == 0)
            diagnostics_.error(loc, "'continue' outside of a loop");
        return;
    }

    if (auto* goto_stmt = std::get_if<GotoStmt>(&stmt.node)) {
        assert(function_ && "goto outside of a function");
        function_->pending_gotos.emplace_back(id, goto_stmt->name);
        return;
    }

    if (auto* label_stmt = std::get_if<LabelStmt>(&stmt.node)) {
        assert(function_ && "label outside of a function");
        const LabelDecl& label = ast_.label(label_stmt->label);
        if (model_.lookup_label(function_->scope, label.name).valid())
            diagnostics_.error(label.loc, "redefinition of label '" + *label.name + "'");
        model_.bind_label(function_->scope, label.name, label_stmt->label);
        model_.label(label_stmt->label).defined = true;
        model_.label(label_stmt->label).loc = label.loc;
        analyze_stmt(label_stmt->stmt);
        return;
    }

    diagnostics_.error(loc, "invalid statement");
}

const Type* Sema::decayed_type(const ExprInfo& info) {
    const Type* type = info.type;
    if (!type)
        return nullptr;
    if (Type::is_array(type))
        return types_.pointer_to(type->element());
    if (Type::is_function(type))
        return types_.pointer_to(type);
    return type;
}

void Sema::set_int_constant(ExprInfo& info, long long value) const {
    info.type = types_.int_type();
    info.category = ValueCategory::RValue;
    info.is_constant = true;
    info.value = wrap_int(value);
    info.null_pointer_constant = info.value == 0;
}

bool Sema::compatible_object_pointers(const Type* a, const Type* b) const {
    if (!Type::is_pointer(a) || !Type::is_pointer(b))
        return false;
    if (a->pointee() != b->pointee())
        return false;
    return Type::is_object(a->pointee()) && Type::is_object(b->pointee());
}

bool Sema::compatible_equality_pointers(const Type* a, const Type* b) const {
    if (!Type::is_pointer(a) || !Type::is_pointer(b))
        return false;
    const Type* a_pointee = a->pointee();
    const Type* b_pointee = b->pointee();
    if (a_pointee == b_pointee)
        return true;
    bool a_void = Type::is_void(a_pointee);
    bool b_void = Type::is_void(b_pointee);
    bool a_object = Type::is_object(a_pointee);
    bool b_object = Type::is_object(b_pointee);
    if (a_void && b_object)
        return true;
    if (b_void && a_object)
        return true;
    return false;
}

void Sema::check_pointer_arithmetic(const Type* pointer, SourceLoc loc) const {
    if (!Type::is_pointer(pointer))
        diagnostics_.error(loc, "pointer arithmetic requires a pointer operand");
    const Type* element = pointer->pointee();
    if (Type::is_void(element))
        diagnostics_.error(loc, "arithmetic on a pointer to void");
    if (Type::is_function(element))
        diagnostics_.error(loc, "arithmetic on a pointer to function");
    if (!is_complete_object(element))
        diagnostics_.error(loc, "arithmetic on a pointer to incomplete type '" +
                                    TypeContext::to_string(element) + "'");
}

bool Sema::assignable(const Type* to, ExprId from) {
    const ExprInfo& from_info = model_.expr(from);
    const Type* from_type = decayed_type(from_info);

    if (Type::is_integer(to))
        return Type::is_integer(from_type);

    if (Type::is_pointer(to)) {
        if (from_info.null_pointer_constant)
            return true;
        if (!Type::is_pointer(from_type))
            return false;
        if (to->pointee() == from_type->pointee())
            return true;
        bool to_void = Type::is_void(to->pointee());
        bool from_void = Type::is_void(from_type->pointee());
        bool to_object = Type::is_object(to->pointee());
        bool from_object = Type::is_object(from_type->pointee());
        if (to_void && from_object)
            return true;
        if (from_void && to_object)
            return true;
        return false;
    }

    if (Type::is_struct(to))
        return from_type == to;

    return false;
}

void Sema::check_scalar(ExprId id, SourceLoc loc, const char* what) {
    const ExprInfo& info = check_expr(id);
    const Type* type = decayed_type(info);
    if (!Type::is_scalar(type)) {
        if (Type::is_void(type))
            diagnostics_.error(loc, std::string(what) + " requires a scalar expression, but found "
                                                        "void");
        diagnostics_.error(loc, std::string(what) + " requires a scalar expression, but found '" +
                                    TypeContext::to_string(type) + "'");
    }
}

void Sema::check_sizeof_type(const Type* type, SourceLoc loc, ExprInfo& info) const {
    if (!is_complete_object(type)) {
        if (Type::is_void(type))
            diagnostics_.error(loc, "invalid application of 'sizeof' to void");
        if (Type::is_function(type))
            diagnostics_.error(loc, "invalid application of 'sizeof' to function type");
        diagnostics_.error(loc, "invalid application of 'sizeof' to incomplete type '" +
                                    TypeContext::to_string(type) + "'");
    }
    set_int_constant(info, static_cast<long long>(layout_.size_of(type)));
}

ExprInfo& Sema::check_expr(ExprId id) {
    ExprInfo& info = model_.expr(id);
    if (info.type)
        return info;

    const Expr& expr = ast_.expr(id);
    SourceLoc loc = expr.loc;

    if (auto* identifier = std::get_if<IdentifierExpr>(&expr.node)) {
        EntityId entity = lookup_ordinary(identifier->name);
        if (!entity.valid())
            diagnostics_.error(loc, "use of undeclared identifier '" + *identifier->name + "'");
        const Entity& resolved = model_.entity(entity);
        if (!resolved.type)
            diagnostics_.error(loc, "internal error: unresolved declaration '" + *identifier->name +
                                        "'");
        info.type = resolved.type;
        info.entity = entity;
        info.category = resolved.kind == EntityKind::Function ? ValueCategory::FunctionDesignator
                                                              : ValueCategory::LValue;
        return info;
    }

    if (auto* constant = std::get_if<ConstantExpr>(&expr.node)) {
        set_int_constant(info, decode_integer_constant(constant->spelling, loc, diagnostics_));
        return info;
    }

    if (auto* string = std::get_if<StringLiteralExpr>(&expr.node)) {
        std::vector<char> bytes = decode_string_literal(string->spelling, loc, diagnostics_);
        info.type = types_.array_of(types_.char_type(), static_cast<uint64_t>(bytes.size()) + 1);
        info.category = ValueCategory::LValue;
        info.string_bytes = std::move(bytes);
        return info;
    }

    if (auto* unary = std::get_if<UnaryExpr>(&expr.node)) {
        ExprInfo& operand = check_expr(unary->operand);
        const Type* type = decayed_type(operand);
        switch (unary->op) {
            case UnaryExpr::Op::Plus:
                if (!Type::is_integer(type))
                    diagnostics_.error(loc, "invalid operand to unary '+'");
                if (operand.is_constant)
                    set_int_constant(info, operand.value);
                else {
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                }
                break;
            case UnaryExpr::Op::Minus:
                if (!Type::is_integer(type))
                    diagnostics_.error(loc, "invalid operand to unary '-'");
                if (operand.is_constant)
                    set_int_constant(info, -operand.value);
                else {
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                }
                break;
            case UnaryExpr::Op::Not:
                if (!Type::is_scalar(type))
                    diagnostics_.error(loc, "invalid operand to unary '!'");
                if (operand.is_constant)
                    set_int_constant(info, operand.value == 0 ? 1 : 0);
                else {
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                }
                break;
            case UnaryExpr::Op::Deref:
                if (!Type::is_pointer(type))
                    diagnostics_.error(loc, "indirection requires a pointer operand");
                if (Type::is_void(type->pointee()))
                    diagnostics_.error(loc, "cannot dereference a pointer to void");
                info.type = type->pointee();
                info.category = Type::is_function(type->pointee())
                                    ? ValueCategory::FunctionDesignator
                                    : ValueCategory::LValue;
                break;
            case UnaryExpr::Op::Addr:
                if (operand.category != ValueCategory::LValue &&
                    operand.category != ValueCategory::FunctionDesignator)
                    diagnostics_.error(loc, "cannot take the address of an rvalue");
                info.type = types_.pointer_to(operand.type);
                info.category = ValueCategory::RValue;
                break;
        }
        return info;
    }

    if (auto* binary = std::get_if<BinaryExpr>(&expr.node)) {
        ExprInfo& left = check_expr(binary->lhs);
        ExprInfo& right = check_expr(binary->rhs);
        const Type* left_type = decayed_type(left);
        const Type* right_type = decayed_type(right);

        switch (binary->op) {
            case BinaryExpr::Op::Mul:
                if (!Type::is_integer(left_type) || !Type::is_integer(right_type))
                    diagnostics_.error(loc, "invalid operands to binary '*'");
                if (left.is_constant && right.is_constant)
                    set_int_constant(info, left.value * right.value);
                else {
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                }
                break;
            case BinaryExpr::Op::Add:
                if (Type::is_integer(left_type) && Type::is_integer(right_type)) {
                    if (left.is_constant && right.is_constant)
                        set_int_constant(info, left.value + right.value);
                    else {
                        info.type = types_.int_type();
                        info.category = ValueCategory::RValue;
                    }
                } else if (Type::is_pointer(left_type) && Type::is_integer(right_type)) {
                    check_pointer_arithmetic(left_type, loc);
                    info.type = left_type;
                    info.category = ValueCategory::RValue;
                } else if (Type::is_integer(left_type) && Type::is_pointer(right_type)) {
                    check_pointer_arithmetic(right_type, loc);
                    info.type = right_type;
                    info.category = ValueCategory::RValue;
                } else {
                    diagnostics_.error(loc, "invalid operands to binary '+'");
                }
                break;
            case BinaryExpr::Op::Sub:
                if (Type::is_integer(left_type) && Type::is_integer(right_type)) {
                    if (left.is_constant && right.is_constant)
                        set_int_constant(info, left.value - right.value);
                    else {
                        info.type = types_.int_type();
                        info.category = ValueCategory::RValue;
                    }
                } else if (Type::is_pointer(left_type) && Type::is_integer(right_type)) {
                    check_pointer_arithmetic(left_type, loc);
                    info.type = left_type;
                    info.category = ValueCategory::RValue;
                } else if (Type::is_pointer(left_type) && Type::is_pointer(right_type)) {
                    if (!compatible_object_pointers(left_type, right_type))
                        diagnostics_.error(loc, "invalid operands to binary '-'");
                    if (!is_complete_object(left_type->pointee()) ||
                        !is_complete_object(right_type->pointee()))
                        diagnostics_.error(loc, "subtraction of pointers to incomplete type");
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                } else {
                    diagnostics_.error(loc, "invalid operands to binary '-'");
                }
                break;
            case BinaryExpr::Op::Less:
                if (Type::is_integer(left_type) && Type::is_integer(right_type)) {
                    if (left.is_constant && right.is_constant)
                        set_int_constant(info, left.value < right.value ? 1 : 0);
                    else {
                        info.type = types_.int_type();
                        info.category = ValueCategory::RValue;
                    }
                } else if (Type::is_pointer(left_type) && Type::is_pointer(right_type) &&
                           compatible_object_pointers(left_type, right_type)) {
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                } else {
                    diagnostics_.error(loc, "invalid operands to binary '<'");
                }
                break;
            case BinaryExpr::Op::Equal:
            case BinaryExpr::Op::NotEqual: {
                bool equal = binary->op == BinaryExpr::Op::Equal;
                if (Type::is_integer(left_type) && Type::is_integer(right_type)) {
                    if (left.is_constant && right.is_constant)
                        set_int_constant(info, (left.value == right.value) == equal ? 1 : 0);
                    else {
                        info.type = types_.int_type();
                        info.category = ValueCategory::RValue;
                    }
                } else if (Type::is_pointer(left_type) && Type::is_pointer(right_type) &&
                           compatible_equality_pointers(left_type, right_type)) {
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                } else if (Type::is_pointer(left_type) && right.null_pointer_constant) {
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                } else if (left.null_pointer_constant && Type::is_pointer(right_type)) {
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                } else {
                    diagnostics_.error(loc, "invalid operands to equality operator");
                }
                break;
            }
            case BinaryExpr::Op::LogicalAnd:
            case BinaryExpr::Op::LogicalOr: {
                bool is_and = binary->op == BinaryExpr::Op::LogicalAnd;
                if (!Type::is_scalar(left_type) || !Type::is_scalar(right_type))
                    diagnostics_.error(loc, "invalid operands to logical operator");
                if (left.is_constant && right.is_constant) {
                    bool l = left.value != 0;
                    bool r = right.value != 0;
                    set_int_constant(info, is_and ? (l && r) : (l || r));
                } else if (left.is_constant) {
                    bool l = left.value != 0;
                    if (is_and && !l)
                        set_int_constant(info, 0);
                    else if (!is_and && l)
                        set_int_constant(info, 1);
                    else {
                        info.type = types_.int_type();
                        info.category = ValueCategory::RValue;
                    }
                } else {
                    info.type = types_.int_type();
                    info.category = ValueCategory::RValue;
                }
                break;
            }
        }
        return info;
    }

    if (auto* assign = std::get_if<AssignExpr>(&expr.node)) {
        ExprInfo& left = check_expr(assign->lhs);
        check_expr(assign->rhs);
        if (left.category != ValueCategory::LValue)
            diagnostics_.error(loc, "expression is not assignable");
        const Type* left_type = left.type;
        if (!Type::is_object(left_type))
            diagnostics_.error(loc, "expression is not assignable");
        if (Type::is_array(left_type))
            diagnostics_.error(loc, "array type is not assignable");
        if (Type::is_struct(left_type))
            diagnostics_.error(loc, "assignment of struct type is not supported");
        if (!is_complete_object(left_type))
            diagnostics_.error(loc, "assignment to incomplete type '" +
                                        TypeContext::to_string(left_type) + "'");
        if (!assignable(left_type, assign->rhs))
            diagnostics_.error(
                loc, "assigning to '" + TypeContext::to_string(left_type) +
                         "' from incompatible type '" +
                         TypeContext::to_string(decayed_type(model_.expr(assign->rhs))) + "'");
        info.type = left_type;
        info.category = ValueCategory::RValue;
        return info;
    }

    if (auto* conditional = std::get_if<ConditionalExpr>(&expr.node)) {
        check_scalar(conditional->cond, loc, "conditional operator");
        ExprInfo& then_info = check_expr(conditional->then_expr);
        ExprInfo& else_info = check_expr(conditional->else_expr);
        const Type* then_type = decayed_type(then_info);
        const Type* else_type = decayed_type(else_info);

        if (Type::is_integer(then_type) && Type::is_integer(else_type)) {
            const ExprInfo& cond_info = model_.expr(conditional->cond);
            if (cond_info.is_constant && then_info.is_constant && else_info.is_constant)
                set_int_constant(info, cond_info.value != 0 ? then_info.value : else_info.value);
            else {
                info.type = types_.int_type();
                info.category = ValueCategory::RValue;
            }
        } else if (Type::is_void(then_type) && Type::is_void(else_type)) {
            info.type = types_.void_type();
            info.category = ValueCategory::Void;
        } else if (Type::is_pointer(then_type) && Type::is_pointer(else_type)) {
            if (!compatible_equality_pointers(then_type, else_type))
                diagnostics_.error(loc, "incompatible operand types in conditional expression");
            if (Type::is_void(then_type->pointee()))
                info.type = then_type;
            else if (Type::is_void(else_type->pointee()))
                info.type = else_type;
            else
                info.type = then_type;
            info.category = ValueCategory::RValue;
        } else if (Type::is_pointer(then_type) && else_info.null_pointer_constant) {
            info.type = then_type;
            info.category = ValueCategory::RValue;
        } else if (then_info.null_pointer_constant && Type::is_pointer(else_type)) {
            info.type = else_type;
            info.category = ValueCategory::RValue;
        } else {
            diagnostics_.error(loc, "incompatible operand types in conditional expression");
        }
        return info;
    }

    if (auto* call = std::get_if<CallExpr>(&expr.node)) {
        ExprInfo& callee = check_expr(call->callee);
        const Type* callee_type = decayed_type(callee);
        const Type* function_type = nullptr;
        if (Type::is_function(callee_type))
            function_type = callee_type;
        else if (Type::is_pointer(callee_type) && Type::is_function(callee_type->pointee()))
            function_type = callee_type->pointee();
        else
            diagnostics_.error(loc, "called object is not a function or function pointer");

        if (call->args.size() != function_type->params().size())
            diagnostics_.error(
                loc, "function call has " + std::to_string(call->args.size()) + " arguments but " +
                         std::to_string(function_type->params().size()) + " are required");

        for (size_t i = 0; i < call->args.size(); ++i) {
            check_expr(call->args[i]);
            if (!assignable(function_type->params()[i], call->args[i]))
                diagnostics_.error(
                    ast_.expr(call->args[i]).loc,
                    "argument " + std::to_string(i + 1) + " has incompatible type '" +
                        TypeContext::to_string(decayed_type(model_.expr(call->args[i]))) + "'");
        }

        info.type = function_type->result();
        info.category =
            Type::is_void(function_type->result()) ? ValueCategory::Void : ValueCategory::RValue;
        return info;
    }

    if (auto* subscript = std::get_if<SubscriptExpr>(&expr.node)) {
        ExprInfo& base = check_expr(subscript->base);
        ExprInfo& index = check_expr(subscript->index);
        const Type* base_type = decayed_type(base);
        const Type* index_type = decayed_type(index);
        const Type* pointer_type = nullptr;
        if (Type::is_pointer(base_type) && Type::is_integer(index_type))
            pointer_type = base_type;
        else if (Type::is_integer(base_type) && Type::is_pointer(index_type))
            pointer_type = index_type;
        else
            diagnostics_.error(loc, "subscripted value is not a pointer or array");
        check_pointer_arithmetic(pointer_type, loc);
        info.type = pointer_type->pointee();
        info.category = ValueCategory::LValue;
        return info;
    }

    if (auto* member = std::get_if<MemberExpr>(&expr.node)) {
        ExprInfo& base = check_expr(member->base);
        StructDecl* record = nullptr;
        ValueCategory category = ValueCategory::RValue;
        if (member->arrow) {
            const Type* base_type = decayed_type(base);
            if (!Type::is_pointer(base_type) || !Type::is_struct(base_type->pointee()))
                diagnostics_.error(loc, "member reference type is not a pointer to struct");
            record = base_type->pointee()->record();
            category = ValueCategory::LValue;
        } else {
            if (!Type::is_struct(base.type))
                diagnostics_.error(loc, "member reference base type is not a struct");
            record = base.type->record();
            category = base.category == ValueCategory::LValue ? ValueCategory::LValue
                                                              : ValueCategory::RValue;
        }

        if (!record || !record->complete)
            diagnostics_.error(loc, "member reference to incomplete struct type");

        for (size_t i = 0; i < record->fields.size(); ++i) {
            if (record->fields[i].name == member->member) {
                info.type = record->fields[i].type;
                info.category = category;
                info.member_record = record;
                info.member_index = static_cast<unsigned>(i);
                return info;
            }
        }

        diagnostics_.error(loc, "no member named '" + *member->member + "' in '" +
                                    TypeContext::to_string(base.type) + "'");
    }

    if (auto* size_of = std::get_if<SizeofExpr>(&expr.node)) {
        if (auto* type_name = std::get_if<SizeofExpr::TypeName>(&size_of->target)) {
            const Type* base = resolve_type_spec(type_name->spec);
            DeclResult result = resolve_declarator(type_name->declarator, base);
            check_sizeof_type(result.type, loc, info);
        } else {
            ExprInfo& operand = check_expr(std::get<ExprId>(size_of->target));
            check_sizeof_type(operand.type, loc, info);
        }
        return info;
    }

    diagnostics_.error(loc, "invalid expression");
}

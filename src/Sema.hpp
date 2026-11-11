#pragma once

#include "AST.hpp"
#include "SemanticModel.hpp"
#include "Support.hpp"
#include "Type.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

class Sema {
public:
    Sema(const AST& ast, SemanticModel& model, TypeContext& types, TypeLayout& layout,
         Diagnostics& diagnostics);

    void analyze();

private:
    struct DeclResult {
        const Type* type = nullptr;
        // Prototype scope of the function type that becomes the declared type.
        // A definition adopts this scope as its function scope.
        ScopeId function_scope;
        std::vector<DeclId> params;
        Symbol name;
        SourceLoc name_loc;
    };

    struct FunctionContext {
        const Type* return_type = nullptr;
        int loop_depth = 0;
        ScopeId scope;
        std::vector<std::pair<StmtId, Symbol>> pending_gotos;
    };

    /// Pushes a lexical scope for the guard's lifetime and pops it afterwards,
    /// so analysis cannot leave the current scope pointing at a dead scope.
    class ScopePush {
    public:
        ScopePush(Sema& sema, ScopeKind kind) : sema_(sema) {
            sema_.push_scope(kind);
        }
        ~ScopePush() {
            sema_.pop_scope();
        }
        ScopePush(const ScopePush&) = delete;
        ScopePush& operator=(const ScopePush&) = delete;

    private:
        Sema& sema_;
    };

    /// Installs a function context and its scope for the guard's lifetime.
    class FunctionScope {
    public:
        FunctionScope(Sema& sema, ScopeId scope, FunctionContext& context)
            : sema_(sema), saved_(sema.current_) {
            sema_.current_ = scope;
            sema_.function_ = &context;
        }
        ~FunctionScope() {
            sema_.function_ = nullptr;
            sema_.current_ = saved_;
        }
        FunctionScope(const FunctionScope&) = delete;
        FunctionScope& operator=(const FunctionScope&) = delete;

    private:
        Sema& sema_;
        ScopeId saved_;
    };

    /// Tracks one loop nesting level for the current function.
    class LoopDepth {
    public:
        explicit LoopDepth(FunctionContext& context) : context_(context) {
            ++context_.loop_depth;
        }
        ~LoopDepth() {
            --context_.loop_depth;
        }
        LoopDepth(const LoopDepth&) = delete;
        LoopDepth& operator=(const LoopDepth&) = delete;

    private:
        FunctionContext& context_;
    };

    const AST& ast_;
    SemanticModel& model_;
    TypeContext& types_;
    TypeLayout& layout_;
    Diagnostics& diagnostics_;

    ScopeId current_;
    FunctionContext* function_ = nullptr;

    // Translation-unit index of external-linkage entities.  Lexical visibility
    // lives in scope bindings; this table implements the C rule that external
    // linkage determines entity identity even for a function first declared in a
    // block, where the file scope has no binding for it yet.
    std::unordered_map<Symbol, EntityId> external_entities_;

    ScopeId push_scope(ScopeKind kind);
    void pop_scope();

    EntityId lookup_ordinary(Symbol name) const;
    StructDecl* lookup_tag(Symbol name) const;

    EntityId declare_file_object(DeclId id, const DeclResult& result, SourceLoc loc);
    EntityId declare_file_function(DeclId id, const DeclResult& result, SourceLoc loc,
                                   bool is_definition);
    EntityId declare_local_object(DeclId id, const DeclResult& result, SourceLoc loc);
    EntityId declare_local_function(DeclId id, const DeclResult& result, SourceLoc loc);
    void record_declaration(DeclId id, EntityId entity, ScopeId scope);

    const Type* resolve_type_spec(TypeSpecId id, bool declare_tag = false);
    const Type* resolve_struct_definition(TypeSpecId id,
                                          const TypeSpec::StructDefinition& definition,
                                          SourceLoc loc);
    const Type* resolve_struct_reference(TypeSpecId id, const TypeSpec::StructReference& reference,
                                         SourceLoc loc, bool declare_tag);
    DeclResult resolve_declarator(DeclaratorId id, const Type* base);
    void adopt_function_scope(ScopeId scope, ScopeId parent);
    const Type* adjust_parameter_type(const Type* type, SourceLoc loc);
    void validate_object_type(const Type* type, SourceLoc loc, const char* what,
                              bool require_complete = true) const;
    void validate_function_type(const Type* type, SourceLoc loc) const;
    void validate_field_type(const Type* type, Symbol name, SourceLoc loc) const;
    bool is_complete_object(const Type* type) const;

    void analyze_top_level(DeclId id);
    void analyze_local_decl(DeclId id);
    void analyze_function(DeclId id, ScopeId function_scope);
    void analyze_compound(StmtId id);
    void analyze_block(StmtId id);
    void analyze_stmt(StmtId id);

    ExprInfo& check_expr(ExprId id);
    void check_scalar(ExprId id, SourceLoc loc, const char* what);
    const Type* decayed_type(const ExprInfo& info);
    bool assignable(const Type* to, ExprId from);
    bool compatible_object_pointers(const Type* a, const Type* b) const;
    bool compatible_equality_pointers(const Type* a, const Type* b) const;
    void check_pointer_arithmetic(const Type* pointer, SourceLoc loc) const;
    void set_int_constant(ExprInfo& info, long long value,
                          bool null_pointer_constant = false) const;
    void check_sizeof_type(const Type* type, SourceLoc loc, ExprInfo& info) const;
};

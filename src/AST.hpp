#pragma once

#include "Support.hpp"
#include "SymbolInternalizer.hpp"
#include "Type.hpp"

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// ---------------------------------------------------------------------------
// Typed 32-bit node identifiers.
//
// Nodes are stored by value in std::vector.  Reallocation may move them, so
// nothing outside the AST ever holds a node pointer or reference across an
// insertion.  Relationships between nodes are expressed only with IDs.
// ---------------------------------------------------------------------------

template <typename Tag> struct NodeId {
    static constexpr uint32_t INVALID = 0xFFFFFFFFu;
    uint32_t index = INVALID;

    NodeId() = default;
    explicit NodeId(uint32_t value) : index(value) {}
    bool valid() const {
        return index != INVALID;
    }
    explicit operator bool() const {
        return valid();
    }
    bool operator==(NodeId other) const {
        return index == other.index;
    }
    bool operator!=(NodeId other) const {
        return !(*this == other);
    }
};

struct ExprTag {};
struct StmtTag {};
struct DeclTag {};
struct DeclaratorTag {};
struct TypeSpecTag {};
struct LabelTag {};

using ExprId = NodeId<ExprTag>;
using StmtId = NodeId<StmtTag>;
using DeclId = NodeId<DeclTag>;
using DeclaratorId = NodeId<DeclaratorTag>;
using TypeSpecId = NodeId<TypeSpecTag>;
using LabelId = NodeId<LabelTag>;

// Typed IDs are used as keys in semantic and code-generation maps.  Hashing the
// index keeps those maps keyed by identity instead of an integer cast at every
// use.
namespace std {
template <typename Tag> struct hash<NodeId<Tag>> {
    size_t operator()(NodeId<Tag> id) const noexcept {
        return hash<uint32_t>{}(id.index);
    }
};
} // namespace std

// ---------------------------------------------------------------------------
// Expression nodes.  Payloads only describe syntax; resolved types and
// declarations are stored in SemanticModel.
// ---------------------------------------------------------------------------

struct IdentifierExpr {
    Symbol name;
};

struct ConstantExpr {
    Symbol spelling;
};

struct StringLiteralExpr {
    Symbol spelling;
};

/// Parentheses are semantically transparent, but they are not syntactically
/// transparent: only the literal token 0 is a null pointer constant, so (0)
/// must remain distinguishable from 0 (S-009).
struct ParenExpr {
    ExprId inner;
};

struct UnaryExpr {
    enum class Op { Plus, Minus, Not, Deref, Addr };
    Op op = Op::Plus;
    ExprId operand;
};

struct BinaryExpr {
    enum class Op { Mul, Add, Sub, Less, Equal, NotEqual, LogicalAnd, LogicalOr };
    Op op = Op::Add;
    ExprId lhs;
    ExprId rhs;
};

struct AssignExpr {
    ExprId lhs;
    ExprId rhs;
};

struct ConditionalExpr {
    ExprId cond;
    ExprId then_expr;
    ExprId else_expr;
};

struct CallExpr {
    ExprId callee;
    std::vector<ExprId> args;
};

struct SubscriptExpr {
    ExprId base;
    ExprId index;
};

struct MemberExpr {
    ExprId base;
    Symbol member;
    bool arrow = false;
};

struct SizeofExpr {
    // sizeof unary-expression or sizeof(type-name).  The variant makes the two
    // syntactic forms mutually exclusive: an expression form cannot carry a
    // type-name payload, and a type-name form cannot carry an operand.  The
    // expression alternative always holds a valid ExprId; the type-name form's
    // declarator is invalid for an abstract type name.
    struct TypeName {
        TypeSpecId spec;
        DeclaratorId declarator; // invalid for an abstract type name
    };

    std::variant<ExprId, TypeName> target;
};

using ExprNode =
    std::variant<IdentifierExpr, ConstantExpr, StringLiteralExpr, ParenExpr, UnaryExpr, BinaryExpr,
                 AssignExpr, ConditionalExpr, CallExpr, SubscriptExpr, MemberExpr, SizeofExpr>;

struct Expr {
    SourceLoc loc;
    ExprNode node;

    Expr(SourceLoc location, ExprNode n) : loc(location), node(std::move(n)) {}
};

// ---------------------------------------------------------------------------
// Statement nodes.
// ---------------------------------------------------------------------------

struct CompoundStmt {
    std::vector<StmtId> items;
};

struct DeclStmt {
    DeclId decl;
};

struct ExprStmt {
    ExprId expr; // invalid for an empty statement
};

struct IfStmt {
    ExprId cond;
    StmtId then_stmt;
    StmtId else_stmt; // invalid when absent
};

struct WhileStmt {
    ExprId cond;
    StmtId body;
};

struct ReturnStmt {
    ExprId expr; // invalid for return;
};

struct BreakStmt {};
struct ContinueStmt {};

struct GotoStmt {
    Symbol name;
};

struct LabelStmt {
    LabelId label;
    StmtId stmt;
};

using StmtNode = std::variant<CompoundStmt, DeclStmt, ExprStmt, IfStmt, WhileStmt, ReturnStmt,
                              BreakStmt, ContinueStmt, GotoStmt, LabelStmt>;

struct Stmt {
    SourceLoc loc;
    StmtNode node;

    Stmt(SourceLoc location, StmtNode n) : loc(location), node(std::move(n)) {}
};

// ---------------------------------------------------------------------------
// Declarators and type specifiers.
// ---------------------------------------------------------------------------

struct NameDeclarator {
    Symbol name;
};

struct PointerDeclarator {
    DeclaratorId inner; // invalid for an abstract "*"
};

struct FunctionDeclarator {
    /// "()": no parameter declarations.  C4 accepts this form and treats it as
    /// a function with no parameters.
    struct NoParameters {};
    /// "(void)": the special parameter list that declares no parameters.
    struct VoidParameters {};
    /// An explicit parameter-type-list.
    using ParameterList = std::vector<DeclId>;

    using Parameters = std::variant<NoParameters, VoidParameters, ParameterList>;

    DeclaratorId inner; // invalid for an abstract function type
    Parameters parameters;
};

using DeclaratorNode = std::variant<NameDeclarator, PointerDeclarator, FunctionDeclarator>;

struct Declarator {
    SourceLoc loc;
    DeclaratorNode node;

    Declarator(SourceLoc location, DeclaratorNode n) : loc(location), node(std::move(n)) {}
};

struct TypeSpec {
    struct Void {};
    struct Char {};
    struct Int {};

    /// "struct tag": a reference to a tag declared elsewhere.  It carries no
    /// fields, which makes an incomplete reference and a definition impossible
    /// to confuse.
    struct StructReference {
        Symbol tag;
    };

    /// "struct tag { ... }" or "struct { ... }": a definition.  The tag is null
    /// for an untagged definition.
    struct StructDefinition {
        Symbol tag;
        std::vector<DeclId> fields;
    };

    using Node = std::variant<Void, Char, Int, StructReference, StructDefinition>;

    SourceLoc loc;
    Node node;

    TypeSpec(SourceLoc location, Node n) : loc(location), node(std::move(n)) {}
};

// ---------------------------------------------------------------------------
// Declaration nodes.
// ---------------------------------------------------------------------------

struct VarDecl {
    TypeSpecId spec;
    DeclaratorId declarator;
};

struct FunctionDecl {
    TypeSpecId spec;
    DeclaratorId declarator;
    StmtId body; // invalid for a prototype
};

struct FieldDecl {
    TypeSpecId spec;
    DeclaratorId declarator;
};

struct ParamDecl {
    TypeSpecId spec;
    DeclaratorId declarator; // may be invalid for an abstract parameter
};

struct TagDecl {
    TypeSpecId spec;
};

using DeclNode = std::variant<VarDecl, FunctionDecl, FieldDecl, ParamDecl, TagDecl>;

struct Decl {
    SourceLoc loc;
    DeclNode node;

    Decl(SourceLoc location, DeclNode n) : loc(location), node(std::move(n)) {}
};

struct LabelDecl {
    SourceLoc loc;
    Symbol name;
};

struct TranslationUnit {
    std::vector<DeclId> decls;
};

// ---------------------------------------------------------------------------
// AST container.  This is the sole owner of all nodes.  All access is by ID.
// ---------------------------------------------------------------------------

class AST {
public:
    template <typename T> ExprId add_expr(SourceLoc loc, T node) {
        uint32_t index = next_index(exprs_.size(), "expression");
        exprs_.emplace_back(loc, ExprNode(std::move(node)));
        return ExprId(index);
    }

    template <typename T> StmtId add_stmt(SourceLoc loc, T node) {
        uint32_t index = next_index(stmts_.size(), "statement");
        stmts_.emplace_back(loc, StmtNode(std::move(node)));
        return StmtId(index);
    }

    template <typename T> DeclId add_decl(SourceLoc loc, T node) {
        uint32_t index = next_index(decls_.size(), "declaration");
        decls_.emplace_back(loc, DeclNode(std::move(node)));
        return DeclId(index);
    }

    template <typename T> DeclaratorId add_declarator(SourceLoc loc, T node) {
        uint32_t index = next_index(declarators_.size(), "declarator");
        declarators_.emplace_back(loc, DeclaratorNode(std::move(node)));
        return DeclaratorId(index);
    }

    TypeSpecId add_type_spec(TypeSpec spec) {
        uint32_t index = next_index(type_specs_.size(), "type specifier");
        type_specs_.push_back(std::move(spec));
        return TypeSpecId(index);
    }

    LabelId add_label(SourceLoc loc, Symbol name) {
        uint32_t index = next_index(labels_.size(), "label");
        labels_.push_back(LabelDecl{loc, name});
        return LabelId(index);
    }

    Expr& expr(ExprId id) {
        check_id(id, exprs_.size(), "expression");
        return exprs_[id.index];
    }
    const Expr& expr(ExprId id) const {
        check_id(id, exprs_.size(), "expression");
        return exprs_[id.index];
    }

    Stmt& stmt(StmtId id) {
        check_id(id, stmts_.size(), "statement");
        return stmts_[id.index];
    }
    const Stmt& stmt(StmtId id) const {
        check_id(id, stmts_.size(), "statement");
        return stmts_[id.index];
    }

    Decl& decl(DeclId id) {
        check_id(id, decls_.size(), "declaration");
        return decls_[id.index];
    }
    const Decl& decl(DeclId id) const {
        check_id(id, decls_.size(), "declaration");
        return decls_[id.index];
    }

    Declarator& declarator(DeclaratorId id) {
        check_id(id, declarators_.size(), "declarator");
        return declarators_[id.index];
    }
    const Declarator& declarator(DeclaratorId id) const {
        check_id(id, declarators_.size(), "declarator");
        return declarators_[id.index];
    }

    TypeSpec& type_spec(TypeSpecId id) {
        check_id(id, type_specs_.size(), "type specifier");
        return type_specs_[id.index];
    }
    const TypeSpec& type_spec(TypeSpecId id) const {
        check_id(id, type_specs_.size(), "type specifier");
        return type_specs_[id.index];
    }

    LabelDecl& label(LabelId id) {
        check_id(id, labels_.size(), "label");
        return labels_[id.index];
    }
    const LabelDecl& label(LabelId id) const {
        check_id(id, labels_.size(), "label");
        return labels_[id.index];
    }

    size_t expr_count() const {
        return exprs_.size();
    }
    size_t stmt_count() const {
        return stmts_.size();
    }
    size_t decl_count() const {
        return decls_.size();
    }
    size_t declarator_count() const {
        return declarators_.size();
    }
    size_t type_spec_count() const {
        return type_specs_.size();
    }
    size_t label_count() const {
        return labels_.size();
    }

    TranslationUnit& translation_unit() {
        return tu_;
    }
    const TranslationUnit& translation_unit() const {
        return tu_;
    }

private:
    template <typename Id> static void check_id(Id id, size_t size, const char* kind) {
        if (!id.valid() || static_cast<size_t>(id.index) >= size)
            throw std::out_of_range(std::string("invalid AST ") + kind + " node id");
    }

    static uint32_t next_index(size_t size, const char* kind) {
        if (size >= static_cast<size_t>(NodeId<ExprTag>::INVALID))
            throw std::length_error(std::string("too many AST ") + kind + " nodes");
        return static_cast<uint32_t>(size);
    }

    std::vector<Expr> exprs_;
    std::vector<Stmt> stmts_;
    std::vector<Decl> decls_;
    std::vector<Declarator> declarators_;
    std::vector<TypeSpec> type_specs_;
    std::vector<LabelDecl> labels_;
    TranslationUnit tu_;
};

// Syntactic declarator queries used by the parser and semantic analysis.
enum class DeclaratorShape { Object, Pointer, Function };
DeclaratorShape declarator_shape(const AST& ast, DeclaratorId id,
                                 DeclaratorShape base = DeclaratorShape::Object);
bool declarator_has_name(const AST& ast, DeclaratorId id);
Symbol declarator_name(const AST& ast, DeclaratorId id);
SourceLoc declarator_name_loc(const AST& ast, DeclaratorId id);
bool declarator_produces_function(const AST& ast, DeclaratorId id);

/// True for both struct type-specifier forms: a tag reference and a definition.
bool is_struct_type_spec(const TypeSpec& spec);

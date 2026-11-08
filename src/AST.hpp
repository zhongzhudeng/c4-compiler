#pragma once

#include "Support.hpp"
#include "SymbolInternalizer.hpp"
#include "Type.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
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
  static constexpr uint32_t Invalid = 0xFFFFFFFFu;
  uint32_t index = Invalid;

  NodeId() = default;
  explicit NodeId(uint32_t value) : index(value) {}
  bool valid() const { return index != Invalid; }
  explicit operator bool() const { return valid(); }
  bool operator==(NodeId other) const { return index == other.index; }
  bool operator!=(NodeId other) const { return !(*this == other); }
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

// ---------------------------------------------------------------------------
// Expression nodes.  Payloads only describe syntax; resolved types and
// declarations are stored in SemanticModel.
// ---------------------------------------------------------------------------

struct IdentifierExpr {
  Symbol name = nullptr;
};

struct ConstantExpr {
  Symbol spelling = nullptr;
};

struct StringLiteralExpr {
  Symbol spelling = nullptr;
};

struct UnaryExpr {
  enum class Op { Plus, Minus, Not, Deref, Addr };
  Op op = Op::Plus;
  ExprId operand;
};

struct BinaryExpr {
  enum class Op {
    Mul,
    Add,
    Sub,
    Less,
    Equal,
    NotEqual,
    LogicalAnd,
    LogicalOr
  };
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
  ExprId thenExpr;
  ExprId elseExpr;
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
  Symbol member = nullptr;
  bool arrow = false;
};

struct SizeofExpr {
  // For sizeof unary-expression, operand is valid.  For sizeof(type-name),
  // typeSpec is valid and operand is invalid.
  ExprId operand;
  TypeSpecId typeSpec;
  DeclaratorId typeDeclarator;
  bool isType = false;
};

using ExprNode = std::variant<IdentifierExpr, ConstantExpr, StringLiteralExpr,
                              UnaryExpr, BinaryExpr, AssignExpr,
                              ConditionalExpr, CallExpr, SubscriptExpr,
                              MemberExpr, SizeofExpr>;

struct Expr {
  SourceLoc loc;
  ExprNode node;

  Expr(SourceLoc location, ExprNode n)
      : loc(location), node(std::move(n)) {}
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
  StmtId thenStmt;
  StmtId elseStmt; // invalid when absent
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
  Symbol name = nullptr;
};

struct LabelStmt {
  LabelId label;
  StmtId stmt;
};

using StmtNode = std::variant<CompoundStmt, DeclStmt, ExprStmt, IfStmt,
                              WhileStmt, ReturnStmt, BreakStmt, ContinueStmt,
                              GotoStmt, LabelStmt>;

struct Stmt {
  SourceLoc loc;
  StmtNode node;

  Stmt(SourceLoc location, StmtNode n)
      : loc(location), node(std::move(n)) {}
};

// ---------------------------------------------------------------------------
// Declarators and type specifiers.
// ---------------------------------------------------------------------------

struct NameDeclarator {
  Symbol name = nullptr;
};

struct PointerDeclarator {
  DeclaratorId inner; // invalid for an abstract "*"
};

struct FunctionDeclarator {
  DeclaratorId inner; // invalid for an abstract function type
  std::vector<DeclId> params;
  bool explicitVoid = false; // printed as (void) rather than ()
};

using DeclaratorNode =
    std::variant<NameDeclarator, PointerDeclarator, FunctionDeclarator>;

struct Declarator {
  SourceLoc loc;
  DeclaratorNode node;

  Declarator(SourceLoc location, DeclaratorNode n)
      : loc(location), node(std::move(n)) {}
};

struct TypeSpec {
  enum class Kind { Void, Char, Int, Struct };
  Kind kind = Kind::Void;
  SourceLoc loc;
  Symbol tag = nullptr;              // struct tag; may be null
  std::vector<DeclId> fields;        // field declarations
  bool hasBraces = false;            // syntactically contains { ... }
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

using DeclNode =
    std::variant<VarDecl, FunctionDecl, FieldDecl, ParamDecl, TagDecl>;

struct Decl {
  SourceLoc loc;
  DeclNode node;

  Decl(SourceLoc location, DeclNode n) : loc(location), node(std::move(n)) {}
};

struct LabelDecl {
  SourceLoc loc;
  Symbol name = nullptr;
};

struct TranslationUnit {
  std::vector<DeclId> decls;
};

// ---------------------------------------------------------------------------
// AST container.  This is the sole owner of all nodes.  All access is by ID.
// ---------------------------------------------------------------------------

class AST {
public:
  template <typename T> ExprId addExpr(SourceLoc loc, T node) {
    exprs_.emplace_back(loc, ExprNode(std::move(node)));
    return ExprId(static_cast<uint32_t>(exprs_.size() - 1));
  }

  template <typename T> StmtId addStmt(SourceLoc loc, T node) {
    stmts_.emplace_back(loc, StmtNode(std::move(node)));
    return StmtId(static_cast<uint32_t>(stmts_.size() - 1));
  }

  template <typename T> DeclId addDecl(SourceLoc loc, T node) {
    decls_.emplace_back(loc, DeclNode(std::move(node)));
    return DeclId(static_cast<uint32_t>(decls_.size() - 1));
  }

  template <typename T> DeclaratorId addDeclarator(SourceLoc loc, T node) {
    declarators_.emplace_back(loc, DeclaratorNode(std::move(node)));
    return DeclaratorId(static_cast<uint32_t>(declarators_.size() - 1));
  }

  TypeSpecId addTypeSpec(TypeSpec spec) {
    typeSpecs_.push_back(std::move(spec));
    return TypeSpecId(static_cast<uint32_t>(typeSpecs_.size() - 1));
  }

  LabelId addLabel(SourceLoc loc, Symbol name) {
    labels_.push_back(LabelDecl{loc, name});
    return LabelId(static_cast<uint32_t>(labels_.size() - 1));
  }

  Expr &expr(ExprId id) {
    assert(id.valid() && id.index < exprs_.size());
    return exprs_[id.index];
  }
  const Expr &expr(ExprId id) const {
    assert(id.valid() && id.index < exprs_.size());
    return exprs_[id.index];
  }

  Stmt &stmt(StmtId id) {
    assert(id.valid() && id.index < stmts_.size());
    return stmts_[id.index];
  }
  const Stmt &stmt(StmtId id) const {
    assert(id.valid() && id.index < stmts_.size());
    return stmts_[id.index];
  }

  Decl &decl(DeclId id) {
    assert(id.valid() && id.index < decls_.size());
    return decls_[id.index];
  }
  const Decl &decl(DeclId id) const {
    assert(id.valid() && id.index < decls_.size());
    return decls_[id.index];
  }

  Declarator &declarator(DeclaratorId id) {
    assert(id.valid() && id.index < declarators_.size());
    return declarators_[id.index];
  }
  const Declarator &declarator(DeclaratorId id) const {
    assert(id.valid() && id.index < declarators_.size());
    return declarators_[id.index];
  }

  TypeSpec &typeSpec(TypeSpecId id) {
    assert(id.valid() && id.index < typeSpecs_.size());
    return typeSpecs_[id.index];
  }
  const TypeSpec &typeSpec(TypeSpecId id) const {
    assert(id.valid() && id.index < typeSpecs_.size());
    return typeSpecs_[id.index];
  }

  LabelDecl &label(LabelId id) {
    assert(id.valid() && id.index < labels_.size());
    return labels_[id.index];
  }
  const LabelDecl &label(LabelId id) const {
    assert(id.valid() && id.index < labels_.size());
    return labels_[id.index];
  }

  size_t exprCount() const { return exprs_.size(); }
  size_t stmtCount() const { return stmts_.size(); }
  size_t declCount() const { return decls_.size(); }
  size_t declaratorCount() const { return declarators_.size(); }
  size_t typeSpecCount() const { return typeSpecs_.size(); }
  size_t labelCount() const { return labels_.size(); }

  TranslationUnit &translationUnit() { return tu_; }
  const TranslationUnit &translationUnit() const { return tu_; }

private:
  std::vector<Expr> exprs_;
  std::vector<Stmt> stmts_;
  std::vector<Decl> decls_;
  std::vector<Declarator> declarators_;
  std::vector<TypeSpec> typeSpecs_;
  std::vector<LabelDecl> labels_;
  TranslationUnit tu_;
};

// Syntactic declarator queries used by the parser and semantic analysis.
enum class DeclaratorShape { Object, Pointer, Function };
DeclaratorShape declaratorShape(const AST &ast, DeclaratorId id,
                                DeclaratorShape base = DeclaratorShape::Object);
bool declaratorHasName(const AST &ast, DeclaratorId id);
Symbol declaratorName(const AST &ast, DeclaratorId id);
SourceLoc declaratorNameLoc(const AST &ast, DeclaratorId id);
bool declaratorProducesFunction(const AST &ast, DeclaratorId id);

#pragma once

#include "AST.hpp"
#include "Support.hpp"
#include "Type.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

/// Value categories are semantic, not syntactic.
enum class ValueCategory { LValue, RValue, FunctionDesignator, Void };

struct FieldInfo {
  Symbol name = nullptr;
  const Type *type = nullptr;
  DeclId decl;
  uint64_t offset = 0;
};

/// A struct/union tag record.  This is a semantic entity, not an AST node.
/// Struct types point at it, which is what permits recursive and incomplete
/// types.  Fields are populated during Sema.
struct StructDecl {
  Symbol tag = nullptr;
  SourceLoc loc;
  std::vector<FieldInfo> fields;
  bool complete = false;
  bool anonymous = false;
  const Type *type = nullptr;
};

struct ExprInfo {
  const Type *type = nullptr;
  ValueCategory category = ValueCategory::RValue;
  bool nullPointerConstant = false;

  // Constant value for integer/character constants and sizeof.
  bool isConstant = false;
  long long value = 0;

  // Resolved identifier / called declaration.
  DeclId decl;

  // Resolved member access.
  StructDecl *memberRecord = nullptr;
  unsigned memberIndex = 0;

  // Resolved goto target.
  LabelId label;

  // Decoded bytes of a string literal (without the terminating NUL).
  std::vector<char> stringBytes;
};

struct DeclInfo {
  const Type *type = nullptr;
  Symbol name = nullptr;
  bool isFunction = false;
  bool isGlobal = false;
  bool isDefinition = false;
  bool isTentative = false;
  std::vector<DeclId> params;
  StructDecl *record = nullptr; // TagDecl
};

struct DeclaratorInfo {
  const Type *type = nullptr;
  std::vector<DeclId> params; // parameters of the outer function type
  bool isFunction = false;
};

struct TypeSpecInfo {
  StructDecl *record = nullptr;
};

struct LabelInfo {
  bool defined = false;
  SourceLoc loc;
};

/// Semantic results keyed by AST node ID.  The model never owns AST nodes and
/// AST nodes never contain semantic results.  It is sized once from the
/// completed AST; Sema fills it in.
class SemanticModel {
public:
  explicit SemanticModel(const AST &ast)
      : exprs_(ast.exprCount()), stmts_(ast.stmtCount()),
        decls_(ast.declCount()), declarators_(ast.declaratorCount()),
        typeSpecs_(ast.typeSpecCount()), labels_(ast.labelCount()),
        gotoTargets_(ast.stmtCount()) {}

  ExprInfo &expr(ExprId id) { return exprs_.at(id.index); }
  const ExprInfo &expr(ExprId id) const { return exprs_.at(id.index); }

  DeclInfo &decl(DeclId id) { return decls_.at(id.index); }
  const DeclInfo &decl(DeclId id) const { return decls_.at(id.index); }

  DeclaratorInfo &declarator(DeclaratorId id) {
    return declarators_.at(id.index);
  }
  const DeclaratorInfo &declarator(DeclaratorId id) const {
    return declarators_.at(id.index);
  }

  TypeSpecInfo &typeSpec(TypeSpecId id) { return typeSpecs_.at(id.index); }
  const TypeSpecInfo &typeSpec(TypeSpecId id) const {
    return typeSpecs_.at(id.index);
  }

  LabelInfo &label(LabelId id) { return labels_.at(id.index); }
  const LabelInfo &label(LabelId id) const { return labels_.at(id.index); }

  LabelId &gotoTarget(StmtId id) { return gotoTargets_.at(id.index); }
  const LabelId &gotoTarget(StmtId id) const {
    return gotoTargets_.at(id.index);
  }

  StructDecl *addStruct(Symbol tag, SourceLoc loc, bool anonymous) {
    auto owned = std::make_unique<StructDecl>();
    owned->tag = tag;
    owned->loc = loc;
    owned->anonymous = anonymous;
    StructDecl *result = owned.get();
    structs_.push_back(std::move(owned));
    return result;
  }

private:
  std::vector<ExprInfo> exprs_;
  std::vector<StmtId> stmts_; // reserved; kept for symmetry/debugging
  std::vector<DeclInfo> decls_;
  std::vector<DeclaratorInfo> declarators_;
  std::vector<TypeSpecInfo> typeSpecs_;
  std::vector<LabelInfo> labels_;
  std::vector<LabelId> gotoTargets_;
  std::vector<std::unique_ptr<StructDecl>> structs_;
};

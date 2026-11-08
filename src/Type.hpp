#pragma once

#include "Support.hpp"
#include "SymbolInternalizer.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace llvm {
class LLVMContext;
class DataLayout;
class Type;
class StructType;
} // namespace llvm

struct StructDecl;

/// Canonical C4 type.  Types are interned by TypeContext, so equality is
/// pointer equality.  Struct types refer to a mutable StructDecl, which is
/// what makes recursive and incomplete types possible.
struct Type {
  enum class Kind { Void, Char, Int, Pointer, Function, Struct, Array };

  Kind kind = Kind::Void;

  // Pointer / Array
  const Type *element = nullptr;
  uint64_t arrayCount = 0;

  // Function
  const Type *result = nullptr;
  std::vector<const Type *> params;

  // Struct
  StructDecl *record = nullptr;

  static bool isInteger(const Type *t) {
    return t && (t->kind == Kind::Char || t->kind == Kind::Int);
  }
  static bool isChar(const Type *t) {
    return t && t->kind == Kind::Char;
  }
  static bool isPointer(const Type *t) {
    return t && t->kind == Kind::Pointer;
  }
  static bool isFunction(const Type *t) {
    return t && t->kind == Kind::Function;
  }
  static bool isStruct(const Type *t) {
    return t && t->kind == Kind::Struct;
  }
  static bool isArray(const Type *t) {
    return t && t->kind == Kind::Array;
  }
  static bool isVoid(const Type *t) { return t && t->kind == Kind::Void; }
  static bool isScalar(const Type *t) {
    return isInteger(t) || isPointer(t);
  }
  static bool isObject(const Type *t) {
    return t && t->kind != Kind::Function && t->kind != Kind::Void;
  }

};

class TypeContext {
public:
  TypeContext();

  const Type *voidType() const { return void_; }
  const Type *charType() const { return char_; }
  const Type *intType() const { return int_; }

  const Type *pointerTo(const Type *element);
  const Type *functionOf(const Type *result, std::vector<const Type *> params);
  const Type *arrayOf(const Type *element, uint64_t count);
  const Type *structType(StructDecl *record);

  /// Human-readable type for diagnostics (not the pretty-printer format).
  static std::string toString(const Type *type);

private:
  const Type *allocate(Type type);
  const Type *void_ = nullptr;
  const Type *char_ = nullptr;
  const Type *int_ = nullptr;

  std::vector<std::unique_ptr<Type>> types_;
  std::map<const Type *, const Type *> pointers_;
  std::map<std::pair<const Type *, std::vector<const Type *>>, const Type *>
      functions_;
  std::map<std::pair<const Type *, uint64_t>, const Type *> arrays_;
  std::map<StructDecl *, const Type *> structs_;
};

/// Bridges C4 types to LLVM's DataLayout.  Both semantic analysis (for
/// sizeof) and code generation use the same object so layouts cannot drift.
class TypeLayout {
public:
  TypeLayout(llvm::LLVMContext &context, const llvm::DataLayout &layout)
      : context_(context), layout_(layout) {}

  llvm::Type *llvmType(const Type *type) const;
  llvm::StructType *llvmStruct(StructDecl *record) const;

  uint64_t sizeOf(const Type *type) const;
  uint64_t alignOf(const Type *type) const;
  uint64_t fieldOffset(StructDecl *record, unsigned index) const;

private:
  llvm::LLVMContext &context_;
  const llvm::DataLayout &layout_;
  mutable std::map<StructDecl *, llvm::StructType *> structTypes_;
};

#include "Type.hpp"

#include "SemanticModel.hpp"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"

#include <cassert>
#include <sstream>

TypeContext::TypeContext() {
  Type v; v.kind = Type::Kind::Void; void_ = allocate(std::move(v));
  Type c; c.kind = Type::Kind::Char; char_ = allocate(std::move(c));
  Type i; i.kind = Type::Kind::Int; int_ = allocate(std::move(i));
}

const Type *TypeContext::allocate(Type type) {
  auto owned = std::make_unique<Type>(std::move(type));
  const Type *result = owned.get();
  types_.push_back(std::move(owned));
  return result;
}

const Type *TypeContext::pointerTo(const Type *element) {
  auto it = pointers_.find(element);
  if (it != pointers_.end())
    return it->second;
  Type t;
  t.kind = Type::Kind::Pointer;
  t.element = element;
  const Type *result = allocate(std::move(t));
  pointers_[element] = result;
  return result;
}

const Type *TypeContext::functionOf(const Type *result,
                                    std::vector<const Type *> params) {
  auto key = std::make_pair(result, std::move(params));
  auto it = functions_.find(key);
  if (it != functions_.end())
    return it->second;
  Type t;
  t.kind = Type::Kind::Function;
  t.result = key.first;
  t.params = key.second;
  const Type *allocated = allocate(std::move(t));
  functions_[std::move(key)] = allocated;
  return allocated;
}

const Type *TypeContext::arrayOf(const Type *element, uint64_t count) {
  auto key = std::make_pair(element, count);
  auto it = arrays_.find(key);
  if (it != arrays_.end())
    return it->second;
  Type t;
  t.kind = Type::Kind::Array;
  t.element = element;
  t.arrayCount = count;
  const Type *result = allocate(std::move(t));
  arrays_[key] = result;
  return result;
}

const Type *TypeContext::structType(StructDecl *record) {
  auto it = structs_.find(record);
  if (it != structs_.end())
    return it->second;
  Type t;
  t.kind = Type::Kind::Struct;
  t.record = record;
  const Type *result = allocate(std::move(t));
  structs_[record] = result;
  return result;
}

std::string TypeContext::toString(const Type *type) {
  if (!type)
    return "<null>";
  switch (type->kind) {
  case Type::Kind::Void:
    return "void";
  case Type::Kind::Char:
    return "char";
  case Type::Kind::Int:
    return "int";
  case Type::Kind::Pointer:
    return toString(type->element) + " *";
  case Type::Kind::Array:
    return toString(type->element) + "[" + std::to_string(type->arrayCount) + "]";
  case Type::Kind::Function: {
    std::string s = toString(type->result) + " (";
    for (size_t i = 0; i < type->params.size(); ++i) {
      if (i)
        s += ", ";
      s += toString(type->params[i]);
    }
    if (type->params.empty())
      s += "void";
    s += ")";
    return s;
  }
  case Type::Kind::Struct: {
    if (type->record && type->record->tag)
      return "struct " + *type->record->tag;
    return "struct <anonymous>";
  }
  }
  return "<type>";
}

llvm::Type *TypeLayout::llvmType(const Type *type) const {
  assert(type && "missing C4 type");
  switch (type->kind) {
  case Type::Kind::Void:
    return llvm::Type::getVoidTy(context_);
  case Type::Kind::Char:
    return llvm::Type::getInt8Ty(context_);
  case Type::Kind::Int:
    return llvm::Type::getInt32Ty(context_);
  case Type::Kind::Pointer:
    return llvm::PointerType::get(context_, 0);
  case Type::Kind::Array:
    return llvm::ArrayType::get(llvmType(type->element), type->arrayCount);
  case Type::Kind::Function: {
    std::vector<llvm::Type *> params;
    params.reserve(type->params.size());
    for (const Type *p : type->params)
      params.push_back(llvmType(p));
    return llvm::FunctionType::get(llvmType(type->result), params, false);
  }
  case Type::Kind::Struct:
    return llvmStruct(type->record);
  }
  return nullptr;
}

llvm::StructType *TypeLayout::llvmStruct(StructDecl *record) const {
  assert(record && "missing struct declaration");
  auto it = structTypes_.find(record);
  if (it != structTypes_.end())
    return it->second;
  std::vector<llvm::Type *> fields;
  fields.reserve(record->fields.size());
  for (const FieldInfo &field : record->fields)
    fields.push_back(llvmType(field.type));
  llvm::StructType *st = llvm::StructType::get(context_, fields, false);
  structTypes_[record] = st;
  return st;
}

uint64_t TypeLayout::sizeOf(const Type *type) const {
  return layout_.getTypeAllocSize(llvmType(type));
}

uint64_t TypeLayout::alignOf(const Type *type) const {
  return layout_.getABITypeAlign(llvmType(type)).value();
}

uint64_t TypeLayout::fieldOffset(StructDecl *record, unsigned index) const {
  llvm::StructType *st = llvmStruct(record);
  return layout_.getStructLayout(st)->getElementOffset(index);
}

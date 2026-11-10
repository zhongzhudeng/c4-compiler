#include "Type.hpp"

#include "SemanticModel.hpp"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"

#include <cassert>
#include <stdexcept>

TypeContext::TypeContext() {
    void_ = allocate(Type::Void{});
    char_ = allocate(Type::Char{});
    int_ = allocate(Type::Int{});
}

const Type* TypeContext::allocate(Type::Node node) {
    // std::make_unique cannot call the private constructor because the
    // allocation happens inside the library template, not in TypeContext.
    std::unique_ptr<Type> owned(new Type(std::move(node)));
    const Type* result = owned.get();
    types_.push_back(std::move(owned));
    return result;
}

const Type* TypeContext::pointer_to(const Type* element) {
    auto it = pointers_.find(element);
    if (it != pointers_.end())
        return it->second;
    const Type* result = allocate(Type::Pointer{element});
    pointers_[element] = result;
    return result;
}

const Type* TypeContext::function_of(const Type* result, std::vector<const Type*> params) {
    auto key = std::make_pair(result, std::move(params));
    auto it = functions_.find(key);
    if (it != functions_.end())
        return it->second;
    const Type* allocated = allocate(Type::Function{key.first, key.second});
    functions_.emplace(std::move(key), allocated);
    return allocated;
}

const Type* TypeContext::array_of(const Type* element, uint64_t count) {
    auto key = std::make_pair(element, count);
    auto it = arrays_.find(key);
    if (it != arrays_.end())
        return it->second;
    const Type* result = allocate(Type::Array{element, count});
    arrays_[key] = result;
    return result;
}

const Type* TypeContext::struct_type(StructDecl* record) {
    auto it = structs_.find(record);
    if (it != structs_.end())
        return it->second;
    const Type* result = allocate(Type::Struct{record});
    structs_[record] = result;
    return result;
}

std::string TypeContext::to_string(const Type* type) {
    if (!type)
        return "<null>";
    if (type->is<Type::Void>())
        return "void";
    if (type->is<Type::Char>())
        return "char";
    if (type->is<Type::Int>())
        return "int";
    if (const auto* pointer = type->get_if<Type::Pointer>())
        return to_string(pointer->pointee) + " *";
    if (const auto* array = type->get_if<Type::Array>())
        return to_string(array->element) + "[" + std::to_string(array->count) + "]";
    if (const auto* function = type->get_if<Type::Function>()) {
        std::string s = to_string(function->result) + " (";
        for (size_t i = 0; i < function->params.size(); ++i) {
            if (i)
                s += ", ";
            s += to_string(function->params[i]);
        }
        if (function->params.empty())
            s += "void";
        s += ")";
        return s;
    }
    if (const auto* structure = type->get_if<Type::Struct>()) {
        if (structure->record && structure->record->tag)
            return "struct " + *structure->record->tag;
        return "struct <anonymous>";
    }
    return "<type>";
}

llvm::Type* TypeLayout::llvm_type(const Type* type) const {
    assert(type && "missing C4 type");
    if (type->is<Type::Void>())
        return llvm::Type::getVoidTy(context_);
    if (type->is<Type::Char>())
        return llvm::Type::getInt8Ty(context_);
    if (type->is<Type::Int>())
        return llvm::Type::getInt32Ty(context_);
    if (type->is<Type::Pointer>())
        return llvm::PointerType::get(context_, 0);
    if (const auto* array = type->get_if<Type::Array>())
        return llvm::ArrayType::get(llvm_type(array->element), array->count);
    if (const auto* function = type->get_if<Type::Function>()) {
        std::vector<llvm::Type*> params;
        params.reserve(function->params.size());
        for (const Type* p : function->params)
            params.push_back(llvm_type(p));
        return llvm::FunctionType::get(llvm_type(function->result), params, false);
    }
    if (const auto* structure = type->get_if<Type::Struct>())
        return llvm_struct(structure->record);
    throw std::logic_error("type: unhandled type category");
}

llvm::StructType* TypeLayout::llvm_struct(StructDecl* record) const {
    assert(record && "missing struct declaration");
    auto it = struct_types_.find(record);
    if (it != struct_types_.end())
        return it->second;
    std::vector<llvm::Type*> fields;
    fields.reserve(record->fields.size());
    for (const FieldInfo& field : record->fields)
        fields.push_back(llvm_type(field.type));
    llvm::StructType* st = llvm::StructType::get(context_, fields, false);
    struct_types_[record] = st;
    return st;
}

uint64_t TypeLayout::size_of(const Type* type) const {
    return layout_.getTypeAllocSize(llvm_type(type));
}

uint64_t TypeLayout::align_of(const Type* type) const {
    return layout_.getABITypeAlign(llvm_type(type)).value();
}

uint64_t TypeLayout::field_offset(StructDecl* record, unsigned index) const {
    llvm::StructType* st = llvm_struct(record);
    return layout_.getStructLayout(st)->getElementOffset(index);
}

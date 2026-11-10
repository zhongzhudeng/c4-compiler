#pragma once

#include "Support.hpp"
#include "SymbolInternalizer.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace llvm {
class LLVMContext;
class DataLayout;
class Type;
class StructType;
} // namespace llvm

struct StructDecl;

/// Canonical C4 type.  Types are interned by TypeContext, so equality is
/// pointer equality.  The variant discriminator is the representation: each
/// alternative owns exactly the data that is meaningful for that category, so
/// there is no separate mutable Kind field and no category can observe another
/// category's payload.  Struct types refer to a mutable StructDecl, which is
/// what makes recursive and incomplete types possible.
struct Type {
    struct Void {};
    struct Char {};
    struct Int {};

    struct Pointer {
        const Type* pointee = nullptr;
    };

    struct Function {
        const Type* result = nullptr;
        std::vector<const Type*> params;
    };

    struct Struct {
        StructDecl* record = nullptr;
    };

    struct Array {
        const Type* element = nullptr;
        uint64_t count = 0;
    };

    using Node = std::variant<Void, Char, Int, Pointer, Function, Struct, Array>;

    template <typename T> bool is() const {
        return std::holds_alternative<T>(node_);
    }
    template <typename T> const T* get_if() const {
        return std::get_if<T>(&node_);
    }

    /// Category-specific accessors.  Each one requires the matching alternative;
    /// std::get fails loudly on a mismatched access instead of exposing an
    /// unrelated field.
    const Type* pointee() const {
        return std::get<Pointer>(node_).pointee;
    }
    const Type* element() const {
        return std::get<Array>(node_).element;
    }
    uint64_t count() const {
        return std::get<Array>(node_).count;
    }
    const Type* result() const {
        return std::get<Function>(node_).result;
    }
    const std::vector<const Type*>& params() const {
        return std::get<Function>(node_).params;
    }
    StructDecl* record() const {
        return std::get<Struct>(node_).record;
    }

    static bool is_integer(const Type* t) {
        return t && (t->is<Char>() || t->is<Int>());
    }
    static bool is_char(const Type* t) {
        return t && t->is<Char>();
    }
    static bool is_pointer(const Type* t) {
        return t && t->is<Pointer>();
    }
    static bool is_function(const Type* t) {
        return t && t->is<Function>();
    }
    static bool is_struct(const Type* t) {
        return t && t->is<Struct>();
    }
    static bool is_array(const Type* t) {
        return t && t->is<Array>();
    }
    static bool is_void(const Type* t) {
        return t && t->is<Void>();
    }
    static bool is_scalar(const Type* t) {
        return is_integer(t) || is_pointer(t);
    }
    static bool is_object(const Type* t) {
        return t && !t->is<Function>() && !t->is<Void>();
    }

private:
    friend class TypeContext;

    explicit Type(Node n) : node_(std::move(n)) {}

    Node node_;
};

class TypeContext {
public:
    TypeContext();

    const Type* void_type() const {
        return void_;
    }
    const Type* char_type() const {
        return char_;
    }
    const Type* int_type() const {
        return int_;
    }

    const Type* pointer_to(const Type* element);
    const Type* function_of(const Type* result, std::vector<const Type*> params);
    const Type* array_of(const Type* element, uint64_t count);
    const Type* struct_type(StructDecl* record);

    /// Human-readable type for diagnostics (not the pretty-printer format).
    static std::string to_string(const Type* type);

private:
    const Type* allocate(Type::Node node);
    const Type* void_ = nullptr;
    const Type* char_ = nullptr;
    const Type* int_ = nullptr;

    std::vector<std::unique_ptr<Type>> types_;
    std::map<const Type*, const Type*> pointers_;
    std::map<std::pair<const Type*, std::vector<const Type*>>, const Type*> functions_;
    std::map<std::pair<const Type*, uint64_t>, const Type*> arrays_;
    std::map<StructDecl*, const Type*> structs_;
};

/// Bridges C4 types to LLVM's DataLayout.  Both semantic analysis (for
/// sizeof) and code generation use the same object so layouts cannot drift.
class TypeLayout {
public:
    TypeLayout(llvm::LLVMContext& context, const llvm::DataLayout& layout)
        : context_(context), layout_(layout) {}

    llvm::Type* llvm_type(const Type* type) const;
    llvm::StructType* llvm_struct(StructDecl* record) const;

    uint64_t size_of(const Type* type) const;
    uint64_t align_of(const Type* type) const;
    uint64_t field_offset(StructDecl* record, unsigned index) const;

private:
    llvm::LLVMContext& context_;
    const llvm::DataLayout& layout_;
    mutable std::map<StructDecl*, llvm::StructType*> struct_types_;
};

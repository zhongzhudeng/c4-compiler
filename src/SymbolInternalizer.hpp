#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>

/// A non-owning, deduplicated handle to a string interned by
/// SymbolInternalizer.  Symbols are pointer-sized and compare by identity, so
/// equal spellings produced by one interner are the same Symbol.  A
/// default-constructed Symbol is null, which represents an absent name or tag;
/// non-null Symbols can only be created by SymbolInternalizer.  A Symbol
/// remains valid until its interner is destroyed, and moving the interner
/// preserves the identity and contents of all Symbols it produced.
class Symbol {
public:
    Symbol() = default;

    explicit operator bool() const {
        return text_ != nullptr;
    }
    bool empty() const {
        return text_ == nullptr || text_->empty();
    }

    const std::string& str() const {
        assert(text_ && "dereferencing a null Symbol");
        return *text_;
    }
    const std::string& operator*() const {
        return str();
    }
    const std::string* operator->() const {
        return &str();
    }

    bool operator==(const Symbol& other) const = default;

    friend bool operator==(const Symbol& symbol, std::nullptr_t) {
        return !symbol;
    }
    friend bool operator==(std::nullptr_t, const Symbol& symbol) {
        return !symbol;
    }

    size_t hash() const noexcept {
        return std::hash<const std::string*>{}(text_);
    }

private:
    friend class SymbolInternalizer;

    explicit Symbol(const std::string* text) : text_(text) {}

    const std::string* text_ = nullptr;
};

namespace std {
template <> struct hash<Symbol> {
    size_t operator()(Symbol symbol) const noexcept {
        return symbol.hash();
    }
};
} // namespace std

/// Owns every interned string and hands out stable Symbol handles.  Equal
/// spellings share one std::string; distinct spellings have distinct Symbols.
/// The element addresses of std::unordered_set are stable across rehashing, so
/// Symbols survive container growth; moving the set moves the nodes without
/// changing those addresses.
class SymbolInternalizer {
public:
    SymbolInternalizer(size_t prealloc_size = 512) : string_set_(prealloc_size) {}

    /// Internalize the given spelling.  The string is copied and does not need
    /// to be preserved by the caller.
    Symbol internalize(std::string_view str);

private:
    std::unordered_set<std::string> string_set_;
};

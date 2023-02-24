#pragma once

#include "SymbolInternalizer.hpp"
#include <cstddef>

class Type {
public:
  enum Kind { Void, Char, Int, Struct };

private:
  Kind kind;
  Symbol struct_iden;
  size_t pointers;

public:
  Type(Kind kind, size_t pointers);
  Type(Symbol struct_iden, size_t pointers);
  bool operator==(Type other) const;
  bool operator!=(Type other) const;
};
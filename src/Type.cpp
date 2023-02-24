#include "Type.hpp"
#include <cstddef>

Type::Type(Kind kind, size_t pointers)
    : kind(kind), struct_iden(nullptr), pointers(pointers) {}

Type::Type(Symbol struct_iden, size_t pointers)
    : kind(Kind::Struct), struct_iden(struct_iden), pointers(pointers) {}

bool Type::operator==(Type other) const {
  return kind == other.kind && pointers == other.pointers &&
         (kind == Struct ? struct_iden == other.struct_iden : true);
}

bool Type::operator!=(Type other) const { return not operator==(other); }
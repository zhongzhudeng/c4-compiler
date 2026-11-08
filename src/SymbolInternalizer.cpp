#include "SymbolInternalizer.hpp"

Symbol SymbolInternalizer::internalize(std::string_view str) {
  // Using the address of unordered_set elements is allowed by the standard:
  // "Rehashing invalidates iterators, changes ordering between elements, and
  // changes which buckets elements appear in, but does not invalidate
  // pointers or references to elements."
  const auto &p = StringSet.emplace(str);
  return Symbol(std::addressof(*p.first));
}

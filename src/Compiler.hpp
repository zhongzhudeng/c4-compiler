#pragma once
#include "SymbolInternalizer.hpp"

class Compiler {
  const char *filename;
  SymbolInternalizer symbolInternalizer;

public:
  Compiler(const char *filename);
  void tokenize();
  void parse();
  void print_ast();
};
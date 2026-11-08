#pragma once

#include "AST.hpp"
#include "SemanticModel.hpp"
#include "Support.hpp"
#include "SymbolInternalizer.hpp"
#include "Type.hpp"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"

#include <memory>
#include <string>

namespace llvm {
class TargetMachine;
}

/// Drives one compiler invocation.  A Compiler object is used for exactly one
/// mode, which keeps the AST and semantic model alive for that pipeline.
class Compiler {
public:
  explicit Compiler(std::string filename);
  ~Compiler();

  int tokenize();
  int parse();
  int printAst();
  int compile(bool optimize);

private:
  std::string filename_;
  std::string triple_;
  SymbolInternalizer symbols_;
  Diagnostics diagnostics_;
  AST ast_;
  TypeContext types_;
  llvm::LLVMContext context_;
  std::unique_ptr<llvm::DataLayout> dataLayout_;
  std::unique_ptr<llvm::TargetMachine> targetMachine_;
  std::unique_ptr<TypeLayout> layout_;
  std::unique_ptr<SemanticModel> model_;

  void initializeTarget();
  void runFrontend();
};

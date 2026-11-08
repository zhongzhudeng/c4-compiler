#pragma once

#include "AST.hpp"
#include "SemanticModel.hpp"
#include "Support.hpp"
#include "SymbolInternalizer.hpp"
#include "Token.hpp"
#include "Type.hpp"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace llvm {
class Module;
} // namespace llvm

/// Requested pipeline depth.  main.cpp maps the argument spelling to this enum
/// once, and run() selects the required stages exactly once.
enum class CompileMode { Tokenize, Parse, PrintAst, Compile, Optimize };

class TokenizedSource;
class ParsedProgram;
class AnalyzedProgram;
class TargetEnvironment;

/// Lexes a file or an in-memory source string.  The result owns the token
/// stream and the SymbolInternalizer its tokens point at.
TokenizedSource tokenizeFile(std::string filename);
TokenizedSource tokenizeString(std::string filename, std::string source);

/// Parses a completed token stream.  The result owns the AST and the symbols
/// its names point at; the token stream itself is discarded.
ParsedProgram parse(TokenizedSource source);

/// Runs semantic analysis over a completed AST.  The result owns the semantic
/// model and the parsed program; types and layout are borrowed and must outlive
/// it.
AnalyzedProgram analyze(ParsedProgram program, TypeContext &types,
                        TypeLayout &layout);

/// Lowers an analyzed program into a verified LLVM module.  The module uses the
/// target's LLVMContext, so it is consistent with the TypeLayout that Sema and
/// CodeGen share.
std::unique_ptr<llvm::Module> generateModule(const AnalyzedProgram &program,
                                             TargetEnvironment &target,
                                             std::string moduleName);

/// mem2reg followed by the project's SCCP, then an LLVM verifier check.
void optimizeModule(llvm::Module &module);

/// Prints the module as textual IR.  An unwritable path is reported through
/// the source diagnostic channel (exit code 1), matching the CLI contract.
void writeModule(const llvm::Module &module, const std::string &outputPath,
                 const std::string &sourceFilename);

/// Executes one CLI invocation.  Source errors throw CompileError; internal
/// failures throw std::exception.  main.cpp maps those to exit codes 1 and 2.
int run(const std::string &filename, CompileMode mode);

/// The tokens of one lexed source together with the symbols they reference.
/// A Symbol is valid only while the interner that produced it is alive, so the
/// interner travels with the token stream instead of being a separate input to
/// later phases.
class TokenizedSource {
public:
  TokenizedSource(TokenizedSource &&) noexcept = default;
  TokenizedSource(const TokenizedSource &) = delete;
  TokenizedSource &operator=(const TokenizedSource &) = delete;

  const std::string &filename() const { return filename_; }
  const std::vector<Token> &tokens() const { return tokens_; }

private:
  friend TokenizedSource tokenizeFile(std::string filename);
  friend TokenizedSource tokenizeString(std::string filename,
                                        std::string source);
  friend ParsedProgram parse(TokenizedSource source);

  explicit TokenizedSource(std::string filename)
      : filename_(std::move(filename)) {}

  std::string filename_;
  SymbolInternalizer symbols_;
  std::vector<Token> tokens_;
};

/// A completed AST and the symbols its names point at.  Tokens are gone: the
/// parser consumed them, and only interned names survive into semantic
/// analysis and code generation.
class ParsedProgram {
public:
  ParsedProgram(ParsedProgram &&) noexcept = default;
  ParsedProgram(const ParsedProgram &) = delete;
  ParsedProgram &operator=(const ParsedProgram &) = delete;

  const std::string &filename() const { return filename_; }
  const AST &ast() const { return ast_; }

private:
  friend ParsedProgram parse(TokenizedSource source);

  ParsedProgram(std::string filename, SymbolInternalizer symbols, AST ast)
      : filename_(std::move(filename)), symbols_(std::move(symbols)),
        ast_(std::move(ast)) {}

  std::string filename_;
  SymbolInternalizer symbols_;
  AST ast_;
};

/// A parsed program with completed semantic analysis.  The model refers to AST
/// node IDs, so it owns the parsed program.  TypeContext and TypeLayout are
/// borrowed because they are shared with code generation; run() constructs
/// them first, which guarantees they outlive this object.
class AnalyzedProgram {
public:
  AnalyzedProgram(AnalyzedProgram &&) noexcept = default;
  AnalyzedProgram(const AnalyzedProgram &) = delete;
  AnalyzedProgram &operator=(const AnalyzedProgram &) = delete;

  const std::string &filename() const { return program_.filename(); }
  const AST &ast() const { return program_.ast(); }
  const SemanticModel &model() const { return model_; }
  TypeContext &types() const { return types_; }
  TypeLayout &layout() const { return layout_; }

private:
  friend AnalyzedProgram analyze(ParsedProgram program, TypeContext &types,
                                 TypeLayout &layout);

  AnalyzedProgram(ParsedProgram program, TypeContext &types, TypeLayout &layout)
      : program_(std::move(program)), model_(program_.ast()), types_(types),
        layout_(layout) {}

  ParsedProgram program_;
  SemanticModel model_;
  TypeContext &types_;
  TypeLayout &layout_;
};

/// The LLVM resources required for target-specific IR generation.  Constructing
/// this object performs the one-time LLVM target registration and derives a
/// DataLayout from the host target machine (with the historical fallback when
/// no machine can be created).  The TypeLayout is built over the same
/// LLVMContext and DataLayout, so Sema and CodeGen cannot observe different
/// layouts.  Frontend-only modes never construct this object.
class TargetEnvironment {
public:
  TargetEnvironment();
  TargetEnvironment(const TargetEnvironment &) = delete;
  TargetEnvironment(TargetEnvironment &&) = delete;
  TargetEnvironment &operator=(const TargetEnvironment &) = delete;

  llvm::LLVMContext &context() { return context_; }
  const llvm::DataLayout &dataLayout() const { return dataLayout_; }
  TypeLayout &layout() { return layout_; }
  const std::string &triple() const { return triple_; }

private:
  llvm::LLVMContext context_;
  std::string triple_;
  llvm::DataLayout dataLayout_;
  TypeLayout layout_;
};

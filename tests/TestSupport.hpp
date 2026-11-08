#pragma once

#include "AST.hpp"
#include "CodeGen.hpp"
#include "Lexer.hpp"
#include "Optimizer.hpp"
#include "Parser.hpp"
#include "Printer.hpp"
#include "SCCP.hpp"
#include "Sema.hpp"
#include "SemanticModel.hpp"
#include "Support.hpp"
#include "SymbolInternalizer.hpp"
#include "Type.hpp"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <exception>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

/// Captures diagnostics written to std::cerr for the lifetime of the object.
class ScopedCerrCapture {
public:
  ScopedCerrCapture() : old_(std::cerr.rdbuf(buffer_.rdbuf())) {}
  ~ScopedCerrCapture() { std::cerr.rdbuf(old_); }

  std::string text() const { return buffer_.str(); }

private:
  std::ostringstream buffer_;
  std::streambuf *old_;
};

/// Convenience front end for unit tests.  Every call starts from a fresh AST.
class TestCompiler {
public:
  bool parse(const std::string &source, std::string *error = nullptr) {
    ast_ = AST();
    ScopedCerrCapture capture;
    Diagnostics diagnostics("test.c");
    try {
      Lexer lexer("test.c", source, symbols_);
      std::vector<Token> tokens = lexer.tokenize();
      Parser parser(ast_, std::move(tokens), diagnostics);
      parser.parseTranslationUnit();
    } catch (const CompileError &) {
      if (error)
        *error = capture.text();
      return false;
    }
    return true;
  }

  bool analyze(const std::string &source, std::string *error = nullptr) {
    ScopedCerrCapture capture;
    if (!parse(source, error))
      return false;
    Diagnostics diagnostics("test.c");
    TypeContext types;
    llvm::LLVMContext context;
    llvm::DataLayout dataLayout("e-m:o-i64:64-i128:128-n32:64-S128");
    TypeLayout layout(context, dataLayout);
    SemanticModel model(ast_);
    try {
      Sema sema(ast_, model, types, layout, diagnostics);
      sema.analyze();
    } catch (const CompileError &) {
      if (error)
        *error = capture.text();
      return false;
    }
    return true;
  }

  bool print(const std::string &source, std::string *output,
             std::string *error = nullptr) {
    ScopedCerrCapture capture;
    if (!parse(source, error))
      return false;
    Diagnostics diagnostics("test.c");
    TypeContext types;
    llvm::LLVMContext context;
    llvm::DataLayout dataLayout("e-m:o-i64:64-i128:128-n32:64-S128");
    TypeLayout layout(context, dataLayout);
    SemanticModel model(ast_);
    try {
      Sema sema(ast_, model, types, layout, diagnostics);
      sema.analyze();
      std::ostringstream stream;
      Printer printer(ast_, model, stream);
      printer.print();
      if (output)
        *output = stream.str();
    } catch (const CompileError &) {
      if (error)
        *error = capture.text();
      return false;
    }
    return true;
  }

  /// Runs the full pipeline and returns the textual LLVM IR.  With optimize
  /// set, mem2reg and our SCCP are applied first.
  bool compile(const std::string &source, bool optimize, std::string *ir,
               std::string *error = nullptr) {
    ScopedCerrCapture capture;
    ast_ = AST();
    Diagnostics diagnostics("test.c");
    try {
      Lexer lexer("test.c", source, symbols_);
      std::vector<Token> tokens = lexer.tokenize();
      Parser parser(ast_, std::move(tokens), diagnostics);
      parser.parseTranslationUnit();

      TypeContext types;
      llvm::LLVMContext context;
      llvm::DataLayout dataLayout("e-m:o-i64:64-i128:128-n32:64-S128");
      TypeLayout layout(context, dataLayout);
      SemanticModel model(ast_);
      Sema sema(ast_, model, types, layout, diagnostics);
      sema.analyze();

      llvm::Module module("test", context);
      module.setDataLayout(dataLayout);
      CodeGen codegen(ast_, model, layout, context, module, diagnostics);
      codegen.emit();

      if (optimize) {
        runMem2Reg(module);
        SCCP sccp(module);
        sccp.run();

        std::string verificationError;
        llvm::raw_string_ostream verificationStream(verificationError);
        if (llvm::verifyModule(module, &verificationStream)) {
          verificationStream.flush();
          throw std::runtime_error("optimized IR failed verification: " +
                                   verificationError);
        }
      }

      std::string text;
      llvm::raw_string_ostream stream(text);
      module.print(stream, nullptr);
      stream.flush();
      if (ir)
        *ir = text;
    } catch (const CompileError &) {
      if (error)
        *error = capture.text();
      return false;
    } catch (const std::exception &exception) {
      if (error)
        *error = exception.what();
      return false;
    }
    return true;
  }

  AST &ast() { return ast_; }
  const AST &ast() const { return ast_; }

private:
  SymbolInternalizer symbols_;
  AST ast_;
};

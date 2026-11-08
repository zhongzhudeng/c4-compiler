#include "Compiler.hpp"

#include "CodeGen.hpp"
#include "Lexer.hpp"
#include "Optimizer.hpp"
#include "Parser.hpp"
#include "Printer.hpp"
#include "SCCP.hpp"
#include "Sema.hpp"

#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"

#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

Compiler::Compiler(std::string filename)
    : filename_(std::move(filename)), diagnostics_(filename_) {
  // Frontend-only modes do not need target registration.  A default
  // DataLayout is enough for their semantic analysis; the real target layout
  // is created lazily by compile().
  dataLayout_ = std::make_unique<llvm::DataLayout>("");
  layout_ = std::make_unique<TypeLayout>(context_, *dataLayout_);
}

Compiler::~Compiler() = default;

void Compiler::initializeTarget() {
  if (targetInitialized_)
    return;
  targetInitialized_ = true;

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();

  llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
  triple_ = triple.str();
  std::string error;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(triple, error);
  if (target) {
    llvm::TargetOptions options;
    targetMachine_.reset(target->createTargetMachine(
        triple, "generic", "", options, std::nullopt));
  }

  auto dataLayout =
      targetMachine_
          ? std::make_unique<llvm::DataLayout>(
                targetMachine_->createDataLayout())
          : std::make_unique<llvm::DataLayout>(
                "e-m:o-i64:64-i128:128-n32:64-S128");
  auto layout = std::make_unique<TypeLayout>(context_, *dataLayout);
  dataLayout_ = std::move(dataLayout);
  layout_ = std::move(layout);
}

void Compiler::runFrontend() {
  Lexer lexer(filename_, symbols_);
  std::vector<Token> tokens = lexer.tokenize();
  Parser parser(ast_, std::move(tokens), diagnostics_);
  parser.parseTranslationUnit();

  model_ = std::make_unique<SemanticModel>(ast_);
  Sema sema(ast_, *model_, types_, *layout_, diagnostics_);
  sema.analyze();
}

int Compiler::tokenize() {
  Lexer lexer(filename_, symbols_);
  lexer.tokenize([](const Token &token) { std::cout << token << '\n'; });
  return 0;
}

int Compiler::parse() {
  runFrontend();
  return 0;
}

int Compiler::printAst() {
  runFrontend();
  Printer printer(ast_, *model_, std::cout);
  printer.print();
  return 0;
}

int Compiler::compile(bool optimize) {
  initializeTarget();
  runFrontend();

  std::string output = filename_;
  std::size_t slash = output.find_last_of("/\\");
  if (slash != std::string::npos)
    output = output.substr(slash + 1);
  if (output.size() >= 2 && output.compare(output.size() - 2, 2, ".c") == 0)
    output.resize(output.size() - 2);
  output += ".ll";

  llvm::Module module(output, context_);
  module.setTargetTriple(llvm::Triple(triple_));
  module.setDataLayout(*dataLayout_);

  CodeGen codegen(ast_, *model_, *layout_, context_, module, diagnostics_);
  codegen.emit();

  if (optimize) {
    runMem2Reg(module);
    SCCP sccp(module);
    sccp.run();

    std::string verificationError;
    llvm::raw_string_ostream stream(verificationError);
    if (llvm::verifyModule(module, &stream)) {
      stream.flush();
      throw std::runtime_error("optimized IR failed verification: " +
                               verificationError);
    }
  }

  std::error_code error;
  llvm::raw_fd_ostream stream(output, error, llvm::sys::fs::OF_Text);
  if (error)
    diagnostics_.error(SourceLoc{1, 1},
                       "cannot write output file '" + output + "'");
  module.print(stream, nullptr);
  stream.close();
  return 0;
}

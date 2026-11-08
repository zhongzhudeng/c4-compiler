#include "Compiler.hpp"

#include "CodeGen.hpp"
#include "Lexer.hpp"
#include "Optimizer.hpp"
#include "Parser.hpp"
#include "Printer.hpp"
#include "SCCP.hpp"
#include "Sema.hpp"

#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"

#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

/// LLVM target registration is process-wide state.  Register once even if a
/// process constructs several TargetEnvironments (for example the tests).
std::string initializeHostTarget() {
  static std::once_flag once;
  std::call_once(once, [] {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
  });
  return llvm::sys::getDefaultTargetTriple();
}

/// The target's layout, or the historical fixed layout when no target machine
/// can be created.  Keeping the fallback preserves the observable IR contract
/// on hosts without a registered target.
llvm::DataLayout targetDataLayout(const std::string &tripleText) {
  llvm::Triple triple(tripleText);
  std::string error;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(triple, error);
  if (target) {
    llvm::TargetOptions options;
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(
        triple, "generic", "", options, std::nullopt));
    if (machine)
      return machine->createDataLayout();
  }
  return llvm::DataLayout("e-m:o-i64:64-i128:128-n32:64-S128");
}

/// <basename>.ll in the current directory; the directory part of the input is
/// stripped and a trailing .c is replaced.  This is the established CLI
/// contract and is independent of the pipeline stages.
std::string outputPath(const std::string &filename) {
  std::string output = filename;
  std::size_t slash = output.find_last_of("/\\");
  if (slash != std::string::npos)
    output = output.substr(slash + 1);
  if (output.size() >= 2 && output.compare(output.size() - 2, 2, ".c") == 0)
    output.resize(output.size() - 2);
  output += ".ll";
  return output;
}

/// Frontend-only modes need a TypeLayout for sizeof, but no target: an empty
/// DataLayout and a private LLVMContext are sufficient and match the previous
/// frontend behavior.  Declaration order is the lifetime order: the layout is
/// destroyed before the context and DataLayout it references.
struct FrontendEnvironment {
  llvm::LLVMContext context;
  llvm::DataLayout dataLayout{""};
  TypeLayout layout{context, dataLayout};
};

} // namespace

TokenizedSource tokenizeFile(std::string filename) {
  TokenizedSource result(std::move(filename));
  Lexer lexer(result.filename_, result.symbols_);
  result.tokens_ = lexer.tokenize();
  return result;
}

TokenizedSource tokenizeString(std::string filename, std::string source) {
  TokenizedSource result(std::move(filename));
  Lexer lexer(result.filename_, std::move(source), result.symbols_);
  result.tokens_ = lexer.tokenize();
  return result;
}

ParsedProgram parse(TokenizedSource source) {
  Diagnostics diagnostics(source.filename_);
  AST ast;
  Parser parser(ast, std::move(source.tokens_), diagnostics);
  parser.parseTranslationUnit();
  // The AST names point into the interner, so ownership moves with the AST.
  return ParsedProgram(source.filename_, std::move(source.symbols_),
                       std::move(ast));
}

AnalyzedProgram analyze(ParsedProgram program, TypeContext &types,
                        TypeLayout &layout) {
  AnalyzedProgram analyzed(std::move(program), types, layout);
  Diagnostics diagnostics(analyzed.filename());
  Sema sema(analyzed.ast(), analyzed.model_, types, layout, diagnostics);
  sema.analyze();
  return analyzed;
}

std::unique_ptr<llvm::Module> generateModule(const AnalyzedProgram &program,
                                             TargetEnvironment &target,
                                             std::string moduleName) {
  auto module = std::make_unique<llvm::Module>(moduleName, target.context());
  module->setTargetTriple(llvm::Triple(target.triple()));
  module->setDataLayout(target.dataLayout());

  CodeGen codegen(program.ast(), program.model(), program.types(),
                  program.layout(), target.context(), *module);
  codegen.emit();
  return module;
}

void optimizeModule(llvm::Module &module) {
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

void writeModule(const llvm::Module &module, const std::string &outputPath,
                 const std::string &sourceFilename) {
  std::error_code error;
  llvm::raw_fd_ostream stream(outputPath, error, llvm::sys::fs::OF_Text);
  if (error) {
    Diagnostics diagnostics(sourceFilename);
    diagnostics.error(SourceLoc{1, 1},
                      "cannot write output file '" + outputPath + "'");
  }
  module.print(stream, nullptr);
  stream.close();
}

TargetEnvironment::TargetEnvironment()
    : triple_(initializeHostTarget()), dataLayout_(targetDataLayout(triple_)),
      layout_(context_, dataLayout_) {}

int run(const std::string &filename, CompileMode mode) {
  switch (mode) {
  case CompileMode::Tokenize: {
    TokenizedSource source = tokenizeFile(filename);
    for (const Token &token : source.tokens()) {
      if (token.Kind != TK_EOI)
        std::cout << token << '\n';
    }
    return 0;
  }
  case CompileMode::Parse: {
    FrontendEnvironment environment;
    TypeContext types;
    analyze(parse(tokenizeFile(filename)), types, environment.layout);
    return 0;
  }
  case CompileMode::PrintAst: {
    FrontendEnvironment environment;
    TypeContext types;
    AnalyzedProgram program =
        analyze(parse(tokenizeFile(filename)), types, environment.layout);
    Printer printer(program.ast(), program.model(), std::cout);
    printer.print();
    return 0;
  }
  case CompileMode::Compile:
  case CompileMode::Optimize: {
    ParsedProgram parsed = parse(tokenizeFile(filename));
    // Backend modes are the only ones that register targets or create a
    // target machine; the environment's TypeLayout is the one Sema uses.
    TargetEnvironment target;
    TypeContext types;
    AnalyzedProgram program =
        analyze(std::move(parsed), types, target.layout());

    std::string output = outputPath(filename);
    std::unique_ptr<llvm::Module> module =
        generateModule(program, target, output);
    if (mode == CompileMode::Optimize)
      optimizeModule(*module);
    writeModule(*module, output, filename);
    return 0;
  }
  }
  throw std::runtime_error("unknown compilation mode");
}

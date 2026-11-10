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

#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

/// LLVM target registration is process-wide state.  Register once even if a
/// process constructs several TargetEnvironments (for example the tests).
std::string initialize_host_target() {
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
llvm::DataLayout target_data_layout(const std::string& triple_text) {
    llvm::Triple triple(triple_text);
    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (target) {
        llvm::TargetOptions options;
        std::unique_ptr<llvm::TargetMachine> machine(
            target->createTargetMachine(triple, "generic", "", options, std::nullopt));
        if (machine)
            return machine->createDataLayout();
    }
    return llvm::DataLayout("e-m:o-i64:64-i128:128-n32:64-S128");
}

/// <basename>.ll in the current directory; the directory part of the input is
/// stripped and a trailing .c is replaced.  This is the established CLI
/// contract and is independent of the pipeline stages.
std::string output_path(const std::string& filename) {
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
    llvm::DataLayout data_layout{""};
    TypeLayout layout{context, data_layout};
};

} // namespace

TokenizedSource tokenize_file(std::string filename) {
    std::ifstream stream(filename, std::ios::binary);
    if (!stream) {
        Diagnostics diagnostics(filename);
        diagnostics.error(SourceLoc{1, 1}, "cannot open input file");
    }
    std::string source;
    source.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return tokenize_string(std::move(filename), std::move(source));
}

TokenizedSource tokenize_string(std::string filename, std::string source) {
    TokenizedSource result(std::move(filename));
    Lexer lexer(result.filename_, std::move(source), result.symbols_);
    result.tokens_ = lexer.tokenize();
    return result;
}

ParsedProgram parse(TokenizedSource source) {
    Diagnostics diagnostics(source.filename_);
    AST ast;
    Parser parser(ast, std::move(source.tokens_), diagnostics);
    parser.parse_translation_unit();
    // The AST names point into the interner, so ownership moves with the AST.
    return ParsedProgram(source.filename_, std::move(source.symbols_), std::move(ast));
}

AnalyzedProgram analyze(ParsedProgram program, TypeContext& types, TypeLayout& layout) {
    AnalyzedProgram analyzed(std::move(program), types, layout);
    Diagnostics diagnostics(analyzed.filename());
    Sema sema(analyzed.ast(), analyzed.model_, types, layout, diagnostics);
    sema.analyze();
    return analyzed;
}

std::unique_ptr<llvm::Module> generate_module(const AnalyzedProgram& program,
                                              TargetEnvironment& target, std::string module_name) {
    auto module = std::make_unique<llvm::Module>(module_name, target.context());
    module->setTargetTriple(llvm::Triple(target.triple()));
    module->setDataLayout(target.data_layout());

    CodeGen codegen(program.ast(), program.model(), program.types(), program.layout(),
                    target.context(), *module);
    codegen.emit();
    return module;
}

void optimize_module(llvm::Module& module) {
    run_mem2_reg(module);
    SCCP sccp(module);
    sccp.run();

    std::string verification_error;
    llvm::raw_string_ostream stream(verification_error);
    if (llvm::verifyModule(module, &stream)) {
        stream.flush();
        throw std::runtime_error("optimized IR failed verification: " + verification_error);
    }
}

void write_module(const llvm::Module& module, const std::string& output_path,
                  const std::string& source_filename) {
    std::error_code error;
    llvm::raw_fd_ostream stream(output_path, error, llvm::sys::fs::OF_Text);
    if (error) {
        Diagnostics diagnostics(source_filename);
        diagnostics.error(SourceLoc{1, 1}, "cannot write output file '" + output_path + "'");
    }
    module.print(stream, nullptr);
    stream.close();
}

TargetEnvironment::TargetEnvironment()
    : triple_(initialize_host_target()), data_layout_(target_data_layout(triple_)),
      layout_(context_, data_layout_) {}

int run(const std::string& filename, CompileMode mode) {
    switch (mode) {
        case CompileMode::Tokenize: {
            TokenizedSource source = tokenize_file(filename);
            for (const Token& token : source.tokens()) {
                if (token.kind != TkEoi)
                    std::cout << token << '\n';
            }
            return 0;
        }
        case CompileMode::Parse: {
            FrontendEnvironment environment;
            TypeContext types;
            analyze(parse(tokenize_file(filename)), types, environment.layout);
            return 0;
        }
        case CompileMode::PrintAst: {
            FrontendEnvironment environment;
            TypeContext types;
            AnalyzedProgram program =
                analyze(parse(tokenize_file(filename)), types, environment.layout);
            // The printer needs only the AST; semantic analysis still runs so that
            // --print-ast rejects the same invalid programs as --parse (task D-001).
            Printer printer(program.ast(), std::cout);
            printer.print();
            return 0;
        }
        case CompileMode::Compile:
        case CompileMode::Optimize: {
            ParsedProgram parsed = parse(tokenize_file(filename));
            // Backend modes are the only ones that register targets or create a
            // target machine; the environment's TypeLayout is the one Sema uses.
            TargetEnvironment target;
            TypeContext types;
            AnalyzedProgram program = analyze(std::move(parsed), types, target.layout());

            std::string output = output_path(filename);
            std::unique_ptr<llvm::Module> module = generate_module(program, target, output);
            if (mode == CompileMode::Optimize)
                optimize_module(*module);
            write_module(*module, output, filename);
            return 0;
        }
    }
    throw std::runtime_error("unknown compilation mode");
}

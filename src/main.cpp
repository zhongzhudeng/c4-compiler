#include "Compiler.hpp"
#include "Support.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {

void usage() {
    std::cerr << "usage: c4 <mode> <file.c>\n"
              << "modes:\n"
              << "  --tokenize <file.c>   print the token stream\n"
              << "  --parse <file.c>      parse and semantically analyze\n"
              << "  --print-ast <file.c>  pretty-print the program\n"
              << "  --compile <file.c>    emit LLVM IR to <file>.ll\n"
              << "  --optimize <file.c>   emit optimized LLVM IR to <file>.ll\n"
              << "  <file.c>              same as --compile\n";
}

struct Invocation {
    CompileMode mode = CompileMode::Compile;
    std::string filename;
};

bool parse_mode(const std::string& text, CompileMode& mode) {
    if (text == "tokenize" || text == "--tokenize") {
        mode = CompileMode::Tokenize;
        return true;
    }
    if (text == "parse" || text == "--parse") {
        mode = CompileMode::Parse;
        return true;
    }
    if (text == "print-ast" || text == "--print-ast") {
        mode = CompileMode::PrintAst;
        return true;
    }
    if (text == "compile" || text == "--compile") {
        mode = CompileMode::Compile;
        return true;
    }
    if (text == "optimize" || text == "--optimize") {
        mode = CompileMode::Optimize;
        return true;
    }
    return false;
}

bool parse_invocation(int argc, char** argv, Invocation& invocation) {
    if (argc == 2 && argv[1][0] != '-') {
        invocation.mode = CompileMode::Compile;
        invocation.filename = argv[1];
        return true;
    }
    if (argc == 3 && parse_mode(argv[1], invocation.mode)) {
        invocation.filename = argv[2];
        return true;
    }
    if (argc == 2) {
        std::string option = argv[1];
        const std::string prefix = "--";
        if (option.rfind(prefix, 0) == 0) {
            std::size_t equals = option.find('=');
            if (equals != std::string::npos &&
                parse_mode(option.substr(0, equals), invocation.mode)) {
                invocation.filename = option.substr(equals + 1);
                return !invocation.filename.empty();
            }
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        usage();
        return 0;
    }

    Invocation invocation;
    if (!parse_invocation(argc, argv, invocation)) {
        usage();
        return 1;
    }

    try {
        return run(invocation.filename, invocation.mode);
    } catch (const CompileError&) {
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "c4: internal error: " << error.what() << '\n';
        return 2;
    }
}

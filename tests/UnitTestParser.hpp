#pragma once
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Sema.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxStmt.hpp"
#include "fmt/core.h"
#include <cstdint>
#include <sstream>

class UnitTestParser {
private:
  uint64_t test_idx=0;

public:
  std::string parse(std::string src) {
    std::string filename = fmt::format("dummy{}.c", test_idx++);
    SymbolInternalizer symbolInternalizer;
    Lexer lexer(filename.data(), std::move(src), symbolInternalizer);
    std::vector<Token> token_stream;
    lexer.tokenize([&](Token tok) { token_stream.emplace_back(tok); });
    Sema sema;
    Parser parser(std::move(token_stream), sema);
    std::ostringstream os;
    parser.parse()->print(os);
    return os.str();
  }
};
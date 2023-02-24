#pragma once
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Sema.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxStmt.hpp"
#include "fmt/core.h"
#include <sstream>

template <typename AST> class UnitTestParser {
public:
  std::string parse(int idx, std::string src) {
    std::string filename = fmt::format("dummy{}.c", idx);
    SymbolInternalizer symbolInternalizer;
    Lexer lexer(filename.data(), std::move(src), symbolInternalizer);
    std::vector<Token> token_stream;
    lexer.tokenize([&](Token tok) { token_stream.emplace_back(tok); });
    Sema sema;
    Parser parser(std::move(token_stream), sema);
    std::unique_ptr<AST> ptr;
    if constexpr (std::is_same_v<AST, Expr>) {
      ptr = parser.parseExpr();
    } else if constexpr (std::is_same_v<AST, DeclaratorDecl>) {
      ptr = parser.parseDecl();
    } else if constexpr (std::is_same_v<AST, Stmt>) {
      sema.enter_func_scope();
      ptr = parser.parseStmt();
      sema.exit_func_scope();
    } else if constexpr (std::is_same_v<AST, Decl>) {
      ptr = parser.parseExternDecl();
    }
    std::ostringstream os;
    ptr->print(os);
    return os.str();
  }
};
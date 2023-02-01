#pragma once
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Sema.hpp"
#include "Syntax.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxExternDecl.hpp"
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
    } else if constexpr (std::is_same_v<AST, Decl>) {
      ptr = parser.parseDecl(DeclKind::ANY);
    } else if constexpr (std::is_same_v<AST, Stmt>) {
      ptr = parser.parseStmt();
    } else if constexpr (std::is_same_v<AST, ExternDecl>) {
      ptr = parser.parseExternDecl();
    }
    std::ostringstream os;
    ptr->print(os);
    return os.str();
  }
};
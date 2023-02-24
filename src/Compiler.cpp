#include "Compiler.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Sema.hpp"
#include "Syntax.hpp"

Compiler::Compiler(const char *filename) : filename(filename) {}

void Compiler::tokenize() {
  Lexer lexer(filename, symbolInternalizer);
  lexer.tokenize([](Token tok) { std::cout << tok << '\n'; });
}

void Compiler::parse() {
  Lexer lexer(filename, symbolInternalizer);
  Sema sema;
  Parser parser(lexer.tokenize(), sema);
  parser.parse([](DeclPtr, bool) {});
}

void Compiler::print_ast() {
  Lexer lexer(filename, symbolInternalizer);
  Sema sema;
  Parser parser(lexer.tokenize(), sema);
  parser.parse([](DeclPtr ast, bool is_end) {
    ast->print(std::cout);
    if (!is_end)
      std::cout << '\n';
  });
}
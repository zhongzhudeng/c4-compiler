#include "Compiler.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Syntax.hpp"
#include "SyntaxExternDecl.hpp"

Compiler::Compiler(const char *filename) : filename(filename) {}

void Compiler::tokenize() {
  Lexer lexer(filename, symbolInternalizer);
  lexer.tokenize([](Token tok) { std::cout << tok << '\n'; });
}

void Compiler::parse() {
  Lexer lexer(filename, symbolInternalizer);
  Parser parser(lexer.tokenize());
  parser.parse([](ExternDeclPtr) {});
}

void Compiler::print_ast() {
  Lexer lexer(filename, symbolInternalizer);
  Parser parser(lexer.tokenize());
  parser.parse([](ExternDeclPtr ast) { ast->print(std::cout); });
}
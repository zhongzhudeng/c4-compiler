#include <getopt.h>

#include "Diagnostic.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "PrettyPrinter.hpp"

using namespace std;

int main(int argc, char **argv) {

  const char *optstring = "";
  static struct option long_options[] = {
      {"tokenize", required_argument, 0, 0},
      {"parse", required_argument, 0, 0},
      {"print-ast", required_argument, 0, 0}};
  int option_index;

  switch (getopt_long(argc, argv, optstring, long_options, &option_index)) {
  case 0: {
    const char *filename = optarg;
    if (filename == nullptr) {
      error("Invalid command line arguments\n");
      return 1;
    }
    switch (option_index) {
    case 0: {
      /// --tokenize
      Lexer lexer(filename);
      lexer.tokenize();
      break;
    }
    case 1: {
      /// --parse
      Lexer lexer(filename);
      Parser parser(lexer.lex());
      parser.parse();
      break;
    }
    case 2: {
      /// print-ast
      Lexer lexer(filename);
      Parser parser(lexer.lex());
      PrettyPrinter pp(parser.parse());
      pp.printAST();
      break;
    }
    default:
      error("Invalid command line arguments\n");
      return 1;
    }
  } break;
  default:
    error("Invalid command line arguments\n");
    return 1;
  }
}
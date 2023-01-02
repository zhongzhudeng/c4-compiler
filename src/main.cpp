#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <sstream>

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
    if (filename == nullptr)
      return 1;
    std::ifstream ifs(filename);
    std::string input(std::istreambuf_iterator<char>{ifs}, {});
    switch (option_index) {
    case 0: {
      /// --tokenize
      Lexer lexer(filename, input);
      lexer.tokenize();
      break;
    }
    case 1: {
      /// --parse
      Lexer lexer(filename, input);
      Parser parser(lexer.lex());
      parser.parse();
      break;
    }
    case 2: {
      /// print-ast
      Lexer lexer(filename, input);
      Parser parser(lexer.lex());
      PrettyPrinter pp(parser.parse());
      pp.printAST();
      break;
    }
    default:
      return 1;
      break;
    }
  }
  default:
    return 1;
    break;
  }
}
#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <sstream>

#include "Lexer.hpp"
#include "parser.h"
#include "PrettyPrinter.hpp"

using namespace std;

int main(int argc, char **argv) {
  int c;
  int option_index;
  static struct option long_options[] = {{"tokenize", required_argument, 0, 0},
                                         {"parse", required_argument, 0, 0}};
  c = getopt_long(argc, argv, "", long_options, &option_index);
  switch (c) {
  case 0:
    if (std::strcmp(long_options[option_index].name, "tokenize") == 0) {
      if (!optarg) return 1;
      std::ifstream t(optarg);
      std::stringstream buffer;
      buffer << t.rdbuf();
      string str(buffer.str());
      Lexer lexer(optarg, str);
      lexer.tokenize();
      break;
    }
    if (std::strcmp(long_options[option_index].name, "parse") == 0) {
      if (!optarg) return 1;
      std::ifstream t(optarg);
      std::stringstream buffer;
      buffer << t.rdbuf();
      string str(buffer.str());
      Lexer lexer(optarg, str);
      Parser parser(lexer.lex());
      parser.parse();
      break;
    }
    if (std::strcmp(long_options[option_index].name, "print-ast") == 0) {
      if (!optarg) return 1;
      std::ifstream t(optarg);
      std::stringstream buffer;
      buffer << t.rdbuf();
      string str(buffer.str());
      Lexer lexer(optarg, str);
      Parser parser(lexer.lex());
      PrettyPrinter pp(parser.parse());
      pp.printAST();
      break;
    }
  default:
    break;
  }
}
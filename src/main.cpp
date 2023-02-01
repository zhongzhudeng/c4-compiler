#include "Compiler.hpp"
#include <exception>
#include <getopt.h>
#include <iostream>

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
      std::cerr << "Invalid command line arguments\n";
      return 1;
    }
    Compiler compiler(filename);
    try {
      switch (option_index) {
      case 0: {
        compiler.tokenize();
        break;
      }
      case 1: {
        compiler.parse();
        break;
      }
      case 2: {
        compiler.print_ast();
        break;
      }
      default:
        std::cerr << "Invalid command line arguments\n";
        return 1;
      }
    } catch (const char *msg) {
      return 1;
    }
  } break;
  default:
    std::cerr << "Invalid command line arguments\n";
    return 1;
  }
}
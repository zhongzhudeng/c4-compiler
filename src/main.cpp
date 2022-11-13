#include <fstream>
#include <iostream>
#include <sstream>

#include "Lexer.hpp"

using namespace std;

int main(int argc, const char **argv) {
  if (strcmp(argv[1], "--tokenize") == 0) {
    string file_name = argv[2];

    std::ifstream t(file_name);
    std::stringstream buffer;
    buffer << t.rdbuf();
    string str(buffer.str());
    Lexer lexer(file_name, str);
    lexer.tokenize();
  }
}
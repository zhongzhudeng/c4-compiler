#pragma once
#include "Syntax.hpp"
#include "Token.hpp"
#include <vector>

#include <experimental/propagate_const>
// Pimpl idiom
class Parser {
  class impl;
  std::experimental::propagate_const<std::unique_ptr<impl>> pImpl;

public:
  explicit Parser(std::vector<Token> token_stream);
  ~Parser();
  ASTPtr parse();
};
#include "Parser.hpp"
#include "ParserImpl.hpp"
#include <memory>

Parser::Parser(std::vector<Token> token_stream)
    : pImpl{std::make_unique<impl>(std::move(token_stream))} {}
Parser::~Parser() = default;

ASTPtr Parser::parse() { return pImpl->parse(); }
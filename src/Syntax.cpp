#include "Syntax.hpp"

AST::AST(Locatable loc) : loc(loc) {}
AST::~AST() {}
const Locatable &AST::getLoc() { return loc; }
#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExternDecl.hpp"
#include "SyntaxStmt.hpp"
#include "Token.hpp"
#include <memory>

ExternDeclPtr Parser::parseExternDecl() {
  Locatable loc(location());
  auto decl = parseDecl();
  auto declarator = decl->getDeclarator();
  if (declarator.getKind() == Declarator::Function) {
    auto compStmt = parseCompoundStmt();
    return std::make_unique<FuncDef>(loc, std::move(decl), std::move(compStmt));
  }
  expect(TK_SEMICOLON, "';' after declaration");
  return decl;
}
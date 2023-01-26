#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExternalDecl.hpp"
#include "SyntaxStmt.hpp"
#include <memory>

ExternDeclPtr Parser::parseExternDecl() {
  Locatable loc(location());
  auto decl = parseDecl();
  auto declarator = decl->getDeclarator();
  if (declarator.getKind() == Declarator::Function) {
    auto compStmt = parseCompoundStmt();
    return std::make_unique<FuncDef>(loc, std::move(decl), std::move(compStmt));
  }
  return decl;
}
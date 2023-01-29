#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExternDecl.hpp"
#include "SyntaxStmt.hpp"
#include "Token.hpp"
#include <memory>

ExternDeclPtr Parser::parseExternDecl() {
  Locatable loc(location());
  DeclPtr decl = parseDecl(DeclKind::ANY);
  const Declarator &declarator = decl->getDeclarator();
  if (declarator.getKind() == Declarator::Function) {
    CompStmtPtr compStmt = parseCompoundStmt();
    return std::make_unique<FuncDef>(loc, std::move(decl), std::move(compStmt));
  }
  expect(TK_SEMICOLON, "';' after declaration");
  return decl;
}
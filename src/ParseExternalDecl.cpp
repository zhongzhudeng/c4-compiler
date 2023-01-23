#include "ParserImpl.hpp"
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"
#include "SyntaxExternalDecl.hpp"
#include "Token.hpp"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

TranslationUnitPtr Parser::impl::parseTranslationUnit(){
  std::vector<ExternalDeclPtr> exDeclList; 
  while (!check(TK_EOI)) {
    exDeclList.push_back(parseExternalDecl());
  }
  return std::make_unique<TranslationUnit>(location(), std::move(exDeclList));
}

ExternalDeclPtr Parser::impl::parseExternalDecl(){
  auto decl = parseDeclaration();
  if (true) {
    return std::make_unique<ExternalDecl>(location(),std::move(decl));
  } else if (check(TK_LBRACE)){
    bump();
    std::vector<ASTPtr> bil;
    if (!check(TK_RBRACE)){
      bil = parseBlockItemList();
    }
    expect(TK_RBRACE, "'}'");
    auto stmt = std::make_unique<CompStmt>(location(), std::move(bil));
    return std::make_unique<ExternalDecl>(location(),std::move(decl), std::move(stmt));
  } else {
    return std::make_unique<ExternalDecl>(location(),std::move(decl));
  }
}
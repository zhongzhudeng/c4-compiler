#include "Diagnostic.hpp"
#include "ParserImpl.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "Token.hpp"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "FirstFollow.def"

DeclarationPtr Parser::impl::parseDeclaration() {
  Locatable loc(location());
  std::vector<DeclarationSpecifierPtr> declSpecs;
  DeclaratorPtr decl = nullptr;

  for (;;) {
    switch (kind()) {
    default:
      errorloc(location(), "Expected struct declaration but got '",
               cursor->Text, "'");
      DECLARATION_SPECIFIER {
        declSpecs.push_back(parseDeclarationSpecifier());
        break;
      }
      DECLARATOR
      goto declarator;
      break;
    case TK_SEMICOLON:
      goto finish;
    }
  }

declarator:
  switch (kind()) {
  default:
    errorloc(location(), "Expected declaration but got '", cursor->Text, "'");
    DECLARATOR
    decl = parseDeclarator(DeclKind::CONCRETE);
  case TK_SEMICOLON:
    goto finish;
  }
finish:
  expect(TK_COLON, "';'");
  return std::make_unique<Declaration>(loc, std::move(declSpecs),
                                       std::move(decl));
}

StructDeclarationPtr Parser::impl::parseStructDeclaration() {
  Locatable loc(location());
  std::vector<SpecifierQualifierPtr> specQuals;
  std::vector<StructDeclaratorPtr> structDeclarators;

  for (;;) {
    switch (kind()) {
    default:
      errorloc(location(), "Expected struct declaration but got '",
               cursor->Text, "'");
      TYPE_SPECIFIER {
        specQuals.push_back(parseSpecifierQualifier());
        break;
      }
      STRUCT_DECLARATOR
      goto declarators;
    case TK_SEMICOLON:
      goto finish;
    }
  }
declarators:
  for (;;) {
    switch (kind()) {
    default:
      errorloc(location(), "Expected struct declarator but got '", cursor->Text,
               "'");
      DECLARATOR {
        structDeclarators.push_back(parseStructDeclarator());
        break;
      }
    case TK_SEMICOLON:
      goto finish;
    }
  }
finish:
  expect(TK_SEMICOLON, "';' in struct declarator");
  return std::make_unique<StructDeclaration>(loc, std::move(specQuals),
                                             std::move(structDeclarators));
}

ParameterDeclarationPtr Parser::impl::parseParameterDeclaration() {
  Locatable loc(location());
  std::vector<DeclarationSpecifierPtr> declSpecs;
  DeclaratorPtr decl = nullptr;

  for (;;) {
    switch (kind()) {
    default:
      errorloc(location(), "Expected struct declaration but got '",
               cursor->Text, "'");
      DECLARATION_SPECIFIER {
        declSpecs.push_back(parseDeclarationSpecifier());
        break;
      }
      DECLARATOR
      goto declarator;
    case TK_COMMA:
    case TK_RPAREN:
      goto finish;
    }
  }

declarator:
  decl = parseDeclarator(DeclKind::ANY);
finish:
  return std::make_unique<ParameterDeclaration>(loc, std::move(declSpecs),
                                                std::move(decl));
}

DeclarationSpecifierPtr Parser::impl::parseSpecifierQualifier() {
  return parseTypeSpecifier();
}

DeclarationSpecifierPtr Parser::impl::parseDeclarationSpecifier() {
  return parseTypeSpecifier();
}

TypeSpecifierPtr Parser::impl::parseTypeSpecifier() {
  Locatable loc(location());
  switch (kind()) {
  default:
    errorloc(location(), "Expected struct declaration list but got '",
             cursor->Text, "'");
  case TK_VOID:
    bump();
    return std::make_unique<VoidSpecifier>(loc);
  case TK_CHAR:
    bump();
    return std::make_unique<CharSpecifier>(loc);
  case TK_INT:
    bump();
    return std::make_unique<IntSpecifier>(loc);
  case TK_STRUCT:
    return parseStructSpecifier();
  }
}

StructSpecifierPtr Parser::impl::parseStructSpecifier() {
  Locatable loc(location());
  bump();
  Symbol tag = nullptr;
  switch (kind()) {
  default:
    errorloc(location(), "Expected struct declaration list but got '",
             cursor->Text, "'");
  case TK_IDENTIFIER:
    tag = cursor->Text;
    bump();
    break;
  case TK_LBRACE:
    break;
  }

  std::vector<StructDeclarationPtr> structDeclarations;
  switch (kind()) {
  default:
    errorloc(location(), "Expected struct declaration list but got '",
             cursor->Text, "'");
  case TK_LBRACE: {
    bump();
    while (!accept(TK_RBRACE)) {
      structDeclarations.push_back(parseStructDeclaration());
    }
    break;
  }
    STRUCT_SPECIFIER_FOLLOW
    break;
  }
  return std::make_unique<StructSpecifier>(loc, tag,
                                           std::move(structDeclarations));
}

StructDeclaratorPtr Parser::impl::parseStructDeclarator() {
  return parseDeclarator(DeclKind::CONCRETE);
}

DeclaratorPtr Parser::impl::parseDeclarator(DeclKind dKind) {
  Locatable loc(location());
  DeclaratorPtr res = nullptr;
  switch (kind()) {
  default:
    errorloc(location(), "Expected declarator but got '", cursor->Text,
             "'");
  case TK_STAR: {
    bump();
    res = std::make_unique<PointerDeclarator>(loc, parseDeclarator(dKind));
  }
  case TK_LPAREN:
    bump();
    if (dKind != DeclKind::ABSTRACT) {
      res = parseDeclarator(dKind);
    } else {
      switch (kind()) {
      default:
        errorloc(location(), "Expected abstract declarator but got '",
                 cursor->Text, "'");
        ABSTRACT_DECLARATOR
        res = parseDeclarator(dKind);
        break;
        DECLARATION_SPECIFIER {
          res = std::make_unique<PrimitiveDeclarator>(loc, nullptr);
          goto param;
        }
      }
    }
    expect(TK_RPAREN, "')'");
  case TK_IDENTIFIER: {
    if (dKind != DeclKind::ABSTRACT) {
      Symbol iden = cursor->Text;
      bump();
      res = std::make_unique<PrimitiveDeclarator>(loc, iden);
    } else {
      errorloc(location(), "Expected declarator but got identifier '",
               cursor->Text, "'");
    }
  }
  }
param:
  if (check(TK_LPAREN)) {
    std::vector<ParameterDeclarationPtr> params;
    do {
      params.push_back(parseParameterDeclaration());
    } while (!check(TK_COMMA));
    expect(TK_RPAREN, "')'");
    return std::make_unique<FunctionDeclarator>(loc, std::move(res),
                                                std::move(params));
  }
  return res;
}
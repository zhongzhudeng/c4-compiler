#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "Token.hpp"
#include <memory>
#include <vector>

#include "FirstFollow.def"

DeclPtr Parser::parseDecl(DeclKind dKind) {
  DeclSpecPtr spec = nullptr;
  Locatable loc(location());
  TokenKind tk = kind();
  bump();
  switch (tk) {
  default:
    errorloc(loc, "Expected type specifier but got `", *cursor->Text, "'");
  case TK_VOID:
    spec = std::make_unique<VoidSpec>(loc);
    break;
  case TK_CHAR:
    spec = std::make_unique<CharSpec>(loc);
    break;
  case TK_INT:
    spec = std::make_unique<IntSpec>(loc);
    break;
  case TK_STRUCT: {
    Symbol iden = nullptr;
    if (check(TK_IDENTIFIER)) {
      iden = cursor->Text;
      bump();
    } else if (!check(TK_LBRACE)) {
      errorloc(location(), "Expected struct declaration list but got `",
               *cursor->Text, "'");
    }
    std::vector<DeclPtr> decls;
    bool is_bracketed = false;
    if (accept(TK_LBRACE)) {
      is_bracketed = true;
      do {
        decls.push_back(parseDecl(DeclKind::CONCRETE));
        expect(TK_SEMICOLON, ";");
      } while (!accept(TK_RBRACE));
    }
    spec =
        std::make_unique<StructSpec>(loc, iden, is_bracketed, std::move(decls));
    break;
  }
  }
  auto decl = parseDeclarator();
  if (decl == nullptr) {
    // TODO check if decl is a valid declarator or abstract declarator
  }
  return std::make_unique<Decl>(loc, std::move(spec), std::move(decl));
}

DeclaratorPtr Parser::parseNonFunDeclarator() {
  Locatable loc(location());
  switch (kind()) {
  case TK_LPAREN: {
    switch (kind_first()) {
    case TK_RPAREN:
    case TK_VOID:
    case TK_CHAR:
    case TK_INT:
    case TK_STRUCT:
      return nullptr;
    default:
      bump();
      auto res = parseDeclarator();
      expect(TK_RPAREN, "')'");
      return res;
    }
  }
  case TK_STAR: {
    bump();
    auto inner = parseDeclarator();
    return std::make_unique<PointerDeclarator>(loc, std::move(inner));
  }
  case TK_IDENTIFIER: {
    auto res = std::make_unique<PrimitiveDeclarator>(loc, cursor->Text);
    bump();
    return res;
  }
  default:
    return nullptr;
  }
}

DeclaratorPtr Parser::parseDeclarator() {
  Locatable loc(location());
  auto res = parseNonFunDeclarator();
  if (accept(TK_LPAREN)) {
    std::vector<DeclPtr> params;
    do {
      params.push_back(parseDecl(DeclKind::ANY));
    } while (accept(TK_COMMA));
    expect(TK_RPAREN, "')'");
    res = std::make_unique<FunctionDeclarator>(loc, std::move(res),
                                               std::move(params));
  }
  return res;
}
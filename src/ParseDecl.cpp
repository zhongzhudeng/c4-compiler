#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"
#include "Token.hpp"
#include <memory>
#include <utility>
#include <vector>

DeclPtr Parser::parseExternDecl() {
  Locatable loc(location());
  DeclaratorDeclPtr decl = parseDecl();
  const Declarator &declarator = decl->getDeclarator();
  if (declarator.getKind() == Declarator::Function) {
    CompStmtPtr stmt = parseCompoundStmt();
    return Action.ActOnFunctionDefinition(
        loc,
        std::unique_ptr<DeclaratorDecl>(
            static_cast<DeclaratorDecl *>(decl.release())),
        std::move(stmt));
  }
  expect(TK_SEMICOLON, "';' after declaration");
  return decl;
}

DeclaratorDeclPtr Parser::parseDecl() {
  Locatable loc(location());
  auto spec = parseDeclSpec();
  auto decl = parseDeclarator();
  return Action.ActOnDeclaration(loc, std::move(spec), std::move(decl));
}

DeclSpecPtr Parser::parseDeclSpec() {
  DeclSpecPtr spec = nullptr;
  Locatable loc(location());
  TokenKind tk = kind();
  Symbol savedText = cursor->Text;
  bump();
  switch (tk) {
  default:
    errorloc(loc, "Expected type specifier but got `", *savedText, "'");
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
    Token iden = *cursor;
    expect(TK_IDENTIFIER, "identifier in struct declaration");
    if (accept(TK_LBRACE)) {
      std::vector<DeclPtr> decls;
      do {
        auto fields = parseDecl();
        decls.push_back(std::move(fields));
        expect(TK_SEMICOLON, ";");
      } while (!accept(TK_RBRACE));
      auto struct_def =
          Action.ActOnStructDefinition(loc, iden, std::move(decls));
      spec = Action.ActOnStructSpec(loc, std::move(struct_def));
    } else {
      spec = Action.ActOnStructSpec(loc, iden.Text);
    }
    break;
  }
  }
  return spec;
}

DeclaratorPtr Parser::parseNonFuncDeclarator() {
  Locatable loc(location());
  switch (kind()) {
  default:
    return std::make_unique<AbstractDeclarator>(loc);
  case TK_IDENTIFIER: {
    auto declarator = std::make_unique<VariableDeclarator>(loc, cursor->Text);
    bump();
    return declarator;
  }
  case TK_LPAREN: {
    switch (kind_first()) {
    case TK_RPAREN:
    case TK_VOID:
    case TK_CHAR:
    case TK_INT:
    case TK_STRUCT:
      return std::make_unique<AbstractDeclarator>(loc);
    default:
      bump();
      auto declarator = parseDeclarator();
      expect(TK_RPAREN, "')'");
      declarator->set_paren();
      return declarator;
    }
  }
  case TK_STAR: {
    bump();
    auto declarator = parseDeclarator();
    if (declarator->getKind() != Declarator::Valued) {
      declarator->add_pointer(loc);
      return declarator;
    }
    auto valued_declarator = std::unique_ptr<ValuedDeclarator>(
        static_cast<ValuedDeclarator *>(declarator.release()));
    if (valued_declarator->getKind() != ValuedDeclarator::FunctionVariable) {
      valued_declarator->add_pointer(loc);
      return valued_declarator;
    }
    auto function_variable_declarator =
        std::unique_ptr<FunctionVariableDeclarator>(
            static_cast<FunctionVariableDeclarator *>(
                valued_declarator.release()));
    function_variable_declarator->add_return_pointer(loc);
    return function_variable_declarator;
  }
  }
}

DeclaratorPtr Parser::parseDeclarator() {
  Locatable loc(location());
  DeclaratorPtr declarator = parseNonFuncDeclarator();
  if (accept(TK_LPAREN)) {
    if (declarator->getKind() != Declarator::Valued)
      errorloc(loc, "Unexpected token `('");
    auto decl = std::unique_ptr<ValuedDeclarator>(
        static_cast<ValuedDeclarator *>(declarator.release()));
    std::vector<DeclaratorDeclPtr> params;
    do {
      params.push_back(parseDecl());
    } while (accept(TK_COMMA));
    expect(TK_RPAREN, "')'");
    if (decl->is_paren())
      return std::make_unique<FunctionVariableDeclarator>(loc, std::move(decl),
                                                          std::move(params));
    return std::make_unique<FunctionDeclarator>(loc, decl->getIdentifier(),
                                                std::move(params));
  }
  return declarator;
}
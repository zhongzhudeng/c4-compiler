#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"
#include "Token.hpp"
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

DeclPtr Parser::parseExternDecl() {
  Locatable loc(location());
  DeclaratorDeclPtr decl = parseDecl();
  if (decl->getDeclarator().is_function()) {
    if (kind() == TK_LBRACE) {
      CompStmtPtr stmt = parseCompoundStmt();
      Action.exit_func_scope();
      return Action.ActOnFunctionDefinition(loc, std::move(decl),
                                            std::move(stmt));
    }
    Action.exit_func_scope();
    decl = Action.ActOnFunctionDecl(std::move(decl));
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
      std::vector<DeclaratorDeclPtr> decls;
      do {
        decls.push_back(std::make_unique<DeclaratorDecl>(
            location(), parseDeclSpec(), parseDeclarator()));
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
    Symbol iden = cursor->Text;
    bump();
    return std::make_unique<VariableDeclarator>(loc, iden);
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
    if (declarator->is_abstract()) {
      declarator->add_pointer(loc);
      return declarator;
    }
    assert(declarator->getKind() == Declarator::Named);
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
  DeclaratorPtr decl = parseNonFuncDeclarator();
  if (accept(TK_LPAREN)) {
    if (!decl->is_named())
      errorloc(loc, "Unexpected token `('");
    auto valued_decl = std::unique_ptr<ValuedDeclarator>(
        static_cast<ValuedDeclarator *>(decl.release()));
    if (valued_decl->getKind() != ValuedDeclarator::Variable)
      errorloc(loc, "Unexpected token `('");
    auto variable_decl = std::unique_ptr<VariableDeclarator>(
        static_cast<VariableDeclarator *>(valued_decl.release()));
    std::vector<DeclaratorDeclPtr> params;
    Action.enter_func_scope();
    do {
      params.push_back(parseDecl());
    } while (accept(TK_COMMA));
    expect(TK_RPAREN, "')'");
    if (variable_decl->is_paren()) {
      Action.exit_func_scope();
      auto decl = Action.ActOnNonFuncDeclarator(std::move(variable_decl));
      auto variable_decl = std::unique_ptr<VariableDeclarator>(
          static_cast<VariableDeclarator *>(valued_decl.release()));
      return std::make_unique<FunctionVariableDeclarator>(
          loc, std::move(variable_decl), std::move(params));
    }
    return std::make_unique<FunctionDeclarator>(
        loc, valued_decl->getIdentifier(), std::move(params));
  }
  return Action.ActOnNonFuncDeclarator(std::move(decl));
}
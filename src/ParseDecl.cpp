#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"
#include "Token.hpp"
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

TranslationUnitPtr Parser::parse() {
  TranslationUnitPtr ast = std::make_unique<TranslationUnit>();
  do {
    ast->insert(ParseExternDecl());
  } while (!is_end());
  return ast;
}

DeclPtr Parser::ParseExternDecl() {
  Locatable loc(location());
  DeclaratorDeclPtr decl = ParseDeclaratorDecl();
  const Declarator &declarator = decl->getDeclarator();
  if (declarator.is_function()) {
    if (kind() == TK_LBRACE) {
      Action.enter_func_scope();
      Action.add_parameters(declarator.as_function());
      CompStmtPtr stmt = ParseCompoundStmt();
      Action.exit_func_scope();
      return Action.ActOnFunctionDefinition(loc, std::move(decl),
                                            std::move(stmt));
    }
    decl = Action.ActOnFunctionDecl(std::move(decl));
  }
  expect(TK_SEMICOLON, "';' after declaration");
  return decl;
}

DeclaratorDeclPtr Parser::ParseDeclaratorDecl() {
  Locatable loc(location());
  auto spec = ParseDeclSpec();
  auto decl = ParseDeclarator();
  return Action.ActOnDeclaration(loc, std::move(spec), std::move(decl));
}

DeclSpecPtr Parser::ParseDeclSpec() {
  Locatable loc(location());
  TokenKind tk = kind();
  Symbol savedText = cursor->Text;
  bump();
  switch (tk) {
  default:
    errorloc(loc, "Expected type specifier but got `", *savedText, "'");
  case TK_VOID:
    return std::make_unique<VoidSpec>(loc);
  case TK_CHAR:
    return std::make_unique<CharSpec>(loc);
  case TK_INT:
    return std::make_unique<IntSpec>(loc);
  case TK_STRUCT: {
    Token iden = *cursor;
    expect(TK_IDENTIFIER, "identifier in struct declaration");
    if (not accept(TK_LBRACE))
      return Action.ActOnStructSpec(loc, iden.Text);
    std::vector<DeclaratorDeclPtr> decls;
    do {
      decls.push_back(ParseDeclaratorDecl());
      expect(TK_SEMICOLON, ";");
    } while (!accept(TK_RBRACE));
    Action.ActOnStructDefinition(loc, iden, std::move(decls));
    return Action.ActOnStructSpec(loc, std::move(struct_def));
  }
  }
}

DeclaratorPtr Parser::ParseNonFuncDeclarator() {
  Locatable loc(location());
  switch (kind()) {
  default:
    return std::make_unique<VariableDeclarator>(loc, nullptr);
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
      return std::make_unique<VariableDeclarator>(loc, nullptr);
    default:
      bump();
      auto declarator = ParseDeclarator();
      expect(TK_RPAREN, "')'");
      declarator->set_paren();
      return declarator;
    }
  }
  case TK_STAR: {
    bump();
    auto declarator = ParseDeclarator();
    if (declarator->is_abstract() || declarator->is_function() ||
        declarator->is_variable()) {
      declarator->add_pointer(loc);
      return declarator;
    }

    assert(declarator->is_function_variable());
    auto function_variable_declarator =
        std::unique_ptr<FunctionVariableDeclarator>(
            static_cast<FunctionVariableDeclarator *>(declarator.release()));
    function_variable_declarator->add_return_pointer(loc);
    return function_variable_declarator;
  }
  }
}

DeclaratorPtr Parser::ParseDeclarator() {
  Locatable loc(location());
  DeclaratorPtr declarator = ParseNonFuncDeclarator();
  if (!accept(TK_LPAREN))
    return Action.ActOnNonFuncDeclarator(std::move(declarator));

  if (not declarator->is_variable())
    errorloc(loc, "Unexpected token `('");
  auto variable_decl = std::unique_ptr<VariableDeclarator>(
      static_cast<VariableDeclarator *>(declarator.release()));
  std::vector<DeclaratorDeclPtr> params;
  do {
    auto decl = ParseDeclaratorDecl();
    assert(not decl->getDeclarator().is_function());
    params.push_back(std::move(decl));
  } while (accept(TK_COMMA));
  expect(TK_RPAREN, "')'");
  if (not variable_decl->is_paren())
    return std::make_unique<FunctionDeclarator>(
        loc, variable_decl->getIdentifier(), std::move(params));
  auto decl = Action.ActOnNonFuncDeclarator(std::move(variable_decl));
  return std::make_unique<FunctionVariableDeclarator>(
      loc,
      std::unique_ptr<VariableDeclarator>(
          static_cast<VariableDeclarator *>(decl.release())),
      std::move(params));
}
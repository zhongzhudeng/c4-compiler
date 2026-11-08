#include <catch2/catch_test_macros.hpp>

#include "TestSupport.hpp"
#include "Compiler.hpp"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"

#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

Symbol firstIdentifier(const std::vector<Token> &tokens) {
  for (const Token &token : tokens)
    if (token.Kind == TK_IDENTIFIER)
      return token.Text;
  return Symbol();
}

} // namespace

TEST_CASE("symbol interning deduplicates equal spellings", "[symbol]") {
  SymbolInternalizer symbols;
  Symbol alpha = symbols.internalize("alpha");
  Symbol alphaAgain = symbols.internalize(std::string("alpha"));
  Symbol beta = symbols.internalize("beta");

  REQUIRE(alpha);
  REQUIRE(alpha == alphaAgain);
  REQUIRE(std::hash<Symbol>{}(alpha) == std::hash<Symbol>{}(alphaAgain));
  REQUIRE(alpha != beta);
  REQUIRE(*alpha == "alpha");
  REQUIRE(*beta == "beta");
}

TEST_CASE("symbols preserve Latin-1 and embedded bytes", "[symbol]") {
  SymbolInternalizer symbols;
  std::string latin1 = "caf\xE9";
  Symbol extended = symbols.internalize(latin1);
  REQUIRE(extended->size() == 4);
  REQUIRE(static_cast<unsigned char>((*extended)[3]) == 0xE9);

  std::string withNul("a\0b", 3);
  Symbol embedded = symbols.internalize(withNul);
  REQUIRE(embedded->size() == 3);
  REQUIRE((*embedded)[0] == 'a');
  REQUIRE((*embedded)[1] == '\0');
  REQUIRE((*embedded)[2] == 'b');
}

TEST_CASE("symbols survive interner growth", "[symbol]") {
  SymbolInternalizer symbols;
  Symbol first = symbols.internalize("first");
  const std::string *address = &*first;

  for (int i = 0; i < 10000; ++i)
    symbols.internalize("symbol-" + std::to_string(i));

  REQUIRE(*first == "first");
  REQUIRE(&*first == address);
  Symbol again = symbols.internalize("first");
  REQUIRE(again == first);
  REQUIRE(&*again == address);
}

TEST_CASE("symbol ownership survives token and AST stage moves", "[symbol]") {
  TokenizedSource tokenized = tokenizeString("move.c", "int alpha;");
  Symbol tokenName = firstIdentifier(tokenized.tokens());
  REQUIRE(tokenName);
  REQUIRE(*tokenName == "alpha");

  ParsedProgram parsed = parse(std::move(tokenized));
  const AST &ast = parsed.ast();
  REQUIRE(ast.translationUnit().decls.size() == 1);
  auto *variable =
      std::get_if<VarDecl>(&ast.decl(ast.translationUnit().decls[0]).node);
  REQUIRE(variable != nullptr);
  Symbol astName = declaratorName(ast, variable->declarator);
  REQUIRE(astName == tokenName);
  REQUIRE(*astName == "alpha");

  llvm::LLVMContext context;
  llvm::DataLayout dataLayout("e-m:o-i64:64-i128:128-n32:64-S128");
  TypeContext types;
  TypeLayout layout(context, dataLayout);
  AnalyzedProgram analyzed = analyze(std::move(parsed), types, layout);

  const SemanticModel &model = analyzed.model();
  EntityId entityId = model.binding(model.fileScope(), astName);
  REQUIRE(entityId.valid());
  const Entity &entity = model.entity(entityId);
  REQUIRE(entity.name == astName);
  REQUIRE(*entity.name == "alpha");
  REQUIRE(analyzed.ast().translationUnit().decls.size() == 1);
}

TEST_CASE("token and symbol value types enforce their invariants",
          "[symbol][token]") {
  static_assert(!std::is_default_constructible_v<Token>);
  static_assert(!std::is_constructible_v<Symbol, const std::string *>);
  static_assert(std::is_trivially_copyable_v<Symbol>);
  static_assert(sizeof(Symbol) == sizeof(const std::string *));

  Symbol null;
  REQUIRE_FALSE(null);
  REQUIRE(null.empty());
  REQUIRE(null == Symbol());
}

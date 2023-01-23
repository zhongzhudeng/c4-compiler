#pragma once

#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include <vector>

class Declaration final : public AST {
  std::vector<DeclarationSpecifierPtr> declarationSpecifiers;
  DeclaratorPtr declarator;

public:
  Declaration(Locatable loc,
              std::vector<DeclarationSpecifierPtr> declarationSpecifiers,
              DeclaratorPtr declarator);
  void accept(ASTPrinter &visitor) override;
  const DeclaratorPtr &getDecl() const;
};

class StructDeclaration final : public AST {
  std::vector<TypeSpecifierPtr> specifierQualifierList;
  std::vector<StructDeclaratorPtr> structDeclaratorList;

public:
  StructDeclaration(Locatable loc,
                    std::vector<TypeSpecifierPtr> specifierQualifierList,
                    std::vector<StructDeclaratorPtr> structDeclaratorList);
  void accept(ASTPrinter &visitor) override;
};

class ParameterDeclaration : public AST {
  std::vector<DeclarationSpecifierPtr> declSpecifierList;
  DeclaratorPtr decl;

public:
  ParameterDeclaration(Locatable loc,
                       std::vector<DeclarationSpecifierPtr> declSpecifierList,
                       DeclaratorPtr decl);
  void accept(ASTPrinter &visitor) override;
};

class TypeSpecifier : public AST {
public:
  TypeSpecifier(Locatable loc);
  virtual ~TypeSpecifier() = 0;
};

class VoidSpecifier final : public TypeSpecifier {

public:
  VoidSpecifier(Locatable loc);
  void accept(ASTPrinter &visitor) override;
};
class CharSpecifier final : public TypeSpecifier {
public:
  CharSpecifier(Locatable loc);
  void accept(ASTPrinter &visitor) override;
};
class IntSpecifier final : public TypeSpecifier {
public:
  IntSpecifier(Locatable loc);
  void accept(ASTPrinter &visitor) override;
};

class StructSpecifier final : public TypeSpecifier {
  Symbol iden;
  std::vector<StructDeclarationPtr> struct_decl_list;

public:
  StructSpecifier(Locatable loc, Symbol iden,
                  std::vector<StructDeclarationPtr> struct_decl_list);
  void accept(ASTPrinter &visitor) override;
};

class Declarator : public AST {
public:
  Declarator(Locatable loc);
  virtual ~Declarator() = 0;
};

class PrimitiveDeclarator final : public Declarator {
public:
  PrimitiveDeclarator(Locatable loc, Symbol tag);
  void accept(ASTPrinter &visitor) override;
};
class PointerDeclarator final : public Declarator {
  DeclaratorPtr decl;

public:
  PointerDeclarator(Locatable loc, DeclaratorPtr decl);
  void accept(ASTPrinter &visitor) override;
};
class FunctionDeclarator final : public Declarator {
  DeclaratorPtr decl;
  std::vector<ParameterDeclarationPtr> params;

public:
  FunctionDeclarator(Locatable loc, DeclaratorPtr decl,
                     std::vector<ParameterDeclarationPtr> params);
  void accept(ASTPrinter &visitor) override;
};
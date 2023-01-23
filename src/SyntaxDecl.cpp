#include "SyntaxDecl.hpp"
#include "Printer.hpp"
#include "Syntax.hpp"
#include <utility>
#include <vector>

Declaration::Declaration(
    Locatable loc, std::vector<DeclarationSpecifierPtr> declarationSpecifiers,
    DeclaratorPtr declarator)
    : AST(loc), declarationSpecifiers(std::move(declarationSpecifiers)),
      declarator(std::move(declarator)) {}

StructDeclaration::StructDeclaration(
    Locatable loc, std::vector<TypeSpecifierPtr> specifierQualifierList,
    std::vector<StructDeclaratorPtr> structDeclaratorList)
    : AST(loc), specifierQualifierList(std::move(specifierQualifierList)),
      structDeclaratorList(std::move(structDeclaratorList)) {}

ParameterDeclaration::ParameterDeclaration(
    Locatable loc, std::vector<DeclarationSpecifierPtr> declSpecifierList,
    DeclaratorPtr decl)
    : AST(loc), declSpecifierList(std::move(declSpecifierList)),
      decl(std::move(decl)) {}

TypeSpecifier::TypeSpecifier(Locatable loc) : AST(loc) {}
TypeSpecifier::~TypeSpecifier() {}
Declarator::Declarator(Locatable loc) : AST(loc) {}
Declarator::~Declarator() {}

VoidSpecifier::VoidSpecifier(Locatable loc) : TypeSpecifier(loc) {}
CharSpecifier::CharSpecifier(Locatable loc) : TypeSpecifier(loc) {}
IntSpecifier::IntSpecifier(Locatable loc) : TypeSpecifier(loc) {}
StructSpecifier::StructSpecifier(
    Locatable loc, Symbol iden,
    std::vector<StructDeclarationPtr> struct_decl_list)
    : TypeSpecifier(loc), iden(iden),
      struct_decl_list(std::move(struct_decl_list)) {}
PrimitiveDeclarator::PrimitiveDeclarator(Locatable loc, Symbol tag)
    : Declarator(loc) {}
PointerDeclarator::PointerDeclarator(Locatable loc, DeclaratorPtr decl)
    : Declarator(loc), decl(std::move(decl)) {}
FunctionDeclarator::FunctionDeclarator(
    Locatable loc, DeclaratorPtr decl,
    std::vector<ParameterDeclarationPtr> params)
    : Declarator(loc), decl(std::move(decl)), params(std::move(params)) {}

void Declaration::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void StructDeclaration::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void ParameterDeclaration::accept(ASTPrinter &visitor) { visitor.Visit(*this); }

void VoidSpecifier::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void CharSpecifier::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void IntSpecifier::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void StructSpecifier::accept(ASTPrinter &visitor) { visitor.Visit(*this); }

void PointerDeclarator::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void FunctionDeclarator::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
void PrimitiveDeclarator::accept(ASTPrinter &visitor) { visitor.Visit(*this); }

const DeclaratorPtr &Declaration::getDecl() const { return const_cast<DeclaratorPtr &>(declarator); };
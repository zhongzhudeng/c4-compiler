#include "SyntaxDecl.hpp"
#include "Diagnostic.hpp"
#include "Syntax.hpp"
#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

Decl::Decl(Locatable loc, DeclSpecPtr declSpec, DeclaratorPtr declarator)
    : ExternDecl(loc, Kind::Decl), declSpec(std::move(declSpec)),
      declarator(std::move(declarator)) {}

const DeclSpec &Decl::getDeclSpec() const {
  return const_cast<DeclSpec &>(*declSpec);
}
bool Decl::isNullDeclarator() const { return declarator == nullptr; }
const Declarator &Decl::getDeclarator() const {
  return const_cast<Declarator &>(*declarator);
}

DeclSpec::DeclSpec(Locatable loc, Kind kind) : loc(loc), kind(kind) {}
Declarator::Declarator(Locatable loc, Kind kind) : loc(loc), kind(kind) {}

const Locatable &DeclSpec::getLoc() { return loc; }
const Locatable &Declarator::getLoc() { return loc; }

DeclSpec::Kind DeclSpec::getKind() const { return kind; }
Declarator::Kind Declarator::getKind() const { return kind; }

VoidSpec::VoidSpec(Locatable loc) : DeclSpec(loc, DeclSpec::Void) {}
CharSpec::CharSpec(Locatable loc) : DeclSpec(loc, DeclSpec::Char) {}
IntSpec::IntSpec(Locatable loc) : DeclSpec(loc, DeclSpec::Int) {}
StructSpec::StructSpec(Locatable loc, Symbol iden, bool is_bracketed,
                       std::vector<DeclPtr> decls)
    : DeclSpec(loc, DeclSpec::Struct), iden(iden), decls(std::move(decls)),
      is_bracketed(is_bracketed) {}
bool StructSpec::isAnoymous() const { return iden == nullptr; }
Symbol StructSpec::getIdentifier() const { return iden; }
bool StructSpec::isBracketed() const { return is_bracketed; }
const std::vector<std::reference_wrapper<const Decl>>
StructSpec::getDecls() const {
  std::vector<std::reference_wrapper<const Decl>> res;
  for (auto &structDecl : decls)
    res.emplace_back(std::cref(*structDecl));
  return res;
}

PrimitiveDeclarator::PrimitiveDeclarator(Locatable loc, Symbol iden)
    : Declarator(loc, Declarator::Primitive), iden(iden) {}
PointerDeclarator::PointerDeclarator(Locatable loc, DeclaratorPtr decl)
    : Declarator(loc, Declarator::Pointer), decl(std::move(decl)) {}
FunctionDeclarator::FunctionDeclarator(Locatable loc, DeclaratorPtr decl,
                                       std::vector<DeclPtr> params)
    : Declarator(loc, Declarator::Function), decl(std::move(decl)),
      params(std::move(params)) {}

Symbol PrimitiveDeclarator::getIdentifier() const { return iden; }

const Declarator &PointerDeclarator::getDeclarator() const {
  return const_cast<Declarator &>(*decl);
}

const Declarator &FunctionDeclarator::getDeclarator() const {
  return const_cast<Declarator &>(*decl);
}
const std::vector<std::reference_wrapper<const Decl>>
FunctionDeclarator::getParams() const {
  std::vector<std::reference_wrapper<const Decl>> res;
  for (auto &param : params)
    res.emplace_back(std::cref(*param));
  return res;
}
#include "SyntaxDecl.hpp"
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxStmt.hpp"
#include <cassert>
#include <utility>
#include <vector>

DeclSpec::DeclSpec(Locatable loc, Type::Kind kind) : loc(loc), kind(kind) {}

DeclSpec::~DeclSpec() {}

const Locatable &DeclSpec::getLoc() { return loc; }

Type::Kind DeclSpec::getKind() const { return kind; }

VoidSpec::VoidSpec(Locatable loc) : DeclSpec(loc, Type::Void) {}

CharSpec::CharSpec(Locatable loc) : DeclSpec(loc, Type::Char) {}

IntSpec::IntSpec(Locatable loc) : DeclSpec(loc, Type::Int) {}

StructSpec::StructSpec(Locatable loc, const StructDef &ref)
    : DeclSpec(loc, Type::Struct), ref(ref), struct_def(nullptr) {}

StructSpec::StructSpec(Locatable loc, StructDefPtr struct_def)
    : DeclSpec(loc, Type::Struct), ref(*struct_def),
      struct_def(std::move(struct_def)) {}

Symbol StructSpec::getIdentifier() const { return ref.getIdentifier(); }
bool StructSpec::has_def() const { return struct_def != nullptr; }
const StructDef &StructSpec::getDef() const {
  return const_cast<class StructDef &>(*struct_def);
}

Declarator::Declarator(Locatable loc, Kind kind) : loc(loc), kind(kind) {}
Declarator::Declarator(Locatable loc, Kind kind,
                       std::vector<Locatable> pointers)
    : loc(loc), kind(kind), pointers(std::move(pointers)) {}
Declarator::Declarator(DeclaratorPtr decl, Kind kind)
    : loc(decl->getLoc()), kind(kind), pointers(std::move(decl->pointers)) {}

Declarator::~Declarator() {}

const Locatable &Declarator::getLoc() const { return loc; }

Declarator::Kind Declarator::getKind() const { return kind; }

const std::vector<Locatable> &Declarator::getPointers() const {
  return pointers;
}
size_t Declarator::pointer_size() const { return pointers.size(); }

void Declarator::add_pointer(Locatable loc) { pointers.push_back(loc); }

void Declarator::set_paren() { paren = true; }

bool Declarator::is_paren() const { return paren; }

bool Declarator::is_abstract() const { return kind == Abstract; }

bool Declarator::is_named() const { return getKind() == Named; }

bool Declarator::is_function() const {
  return getKind() == Named &&
         static_cast<const NamedDeclarator &>(*this).getKind() ==
             NamedDeclarator::Function;
}

const VariableDeclarator &Declarator::as_variable_declarator() const {
  assert(kind == Named);
  const NamedDeclarator &named = static_cast<const NamedDeclarator &>(*this);
  assert(named.getKind() == NamedDeclarator::Valued);
  const ValuedDeclarator &valued = static_cast<const ValuedDeclarator &>(named);
  assert(valued.getKind() == ValuedDeclarator::Variable);
  return static_cast<const VariableDeclarator &>(valued);
}

NamedDeclarator::NamedDeclarator(Locatable loc, Symbol iden, Kind kind)
    : Declarator(loc, Declarator::Named), iden(iden), kind(kind) {}
NamedDeclarator::NamedDeclarator(Locatable loc, Symbol iden, Kind kind,
                                 std::vector<Locatable> pointers)
    : Declarator(loc, Declarator::Named, std::move(pointers)), iden(iden),
      kind(kind) {}
NamedDeclarator::~NamedDeclarator() {}
NamedDeclarator::Kind NamedDeclarator::getKind() const { return kind; }
Symbol NamedDeclarator::getIdentifier() const { return iden; }

ValuedDeclarator::ValuedDeclarator(Locatable loc, Symbol iden, Kind kind)
    : NamedDeclarator(loc, iden, NamedDeclarator::Valued), kind(kind) {}
ValuedDeclarator::ValuedDeclarator(Locatable loc, Symbol iden, Kind kind,
                                   std::vector<Locatable> pointers)
    : NamedDeclarator(loc, iden, NamedDeclarator::Valued, std::move(pointers)),
      kind(kind) {}
ValuedDeclarator::~ValuedDeclarator() {}
ValuedDeclarator::Kind ValuedDeclarator::getKind() const { return kind; }

VariableDeclarator::VariableDeclarator(Locatable loc, Symbol iden)
    : ValuedDeclarator(loc, iden, Kind::Variable) {}

FunctionVariableDeclarator::FunctionVariableDeclarator(
    Locatable loc, VariableDeclaratorPtr decl,
    std::vector<DeclaratorDeclPtr> params)
    : ValuedDeclarator(loc, decl->getIdentifier(), Kind::FunctionVariable,
                       decl->getPointers()),
      params(std::move(params)) {}

size_t FunctionVariableDeclarator::return_pointer_size() const {
  return return_pointers.size();
}
void FunctionVariableDeclarator::add_return_pointer(Locatable loc) {
  return_pointers.push_back(loc);
}

const std::vector<std::reference_wrapper<const DeclaratorDecl>>
FunctionVariableDeclarator::getParams() const {
  std::vector<std::reference_wrapper<const DeclaratorDecl>> res;
  for (auto &param : params)
    res.emplace_back(std::cref(*param));
  return res;
}

FunctionDeclarator::FunctionDeclarator(Locatable loc, Symbol iden,
                                       std::vector<DeclaratorDeclPtr> params)
    : NamedDeclarator(loc, iden, Kind::Function), params(std::move(params)) {}

const std::vector<std::reference_wrapper<const DeclaratorDecl>>
FunctionDeclarator::getParams() const {
  std::vector<std::reference_wrapper<const DeclaratorDecl>> res;
  for (auto &param : params)
    res.emplace_back(std::cref(*param));
  return res;
}

AbstractDeclarator::AbstractDeclarator(Locatable loc)
    : Declarator(loc, Declarator::Abstract) {}

Decl::Decl(Locatable loc, Kind kind) : loc(loc), kind(kind) {}
Decl::~Decl() {}
const Locatable &Decl::getLoc() const { return loc; }
Decl::Kind Decl::getKind() const { return kind; }

LabelDecl::LabelDecl(Locatable loc, Symbol iden)
    : Decl(loc, Decl::LabelDecl), iden(iden) {}
Symbol LabelDecl::getIdentifier() const { return iden; }

DeclaratorDecl::DeclaratorDecl(Locatable loc, DeclSpecPtr declSpec,
                               DeclaratorPtr declarator)
    : Decl(loc, Decl::Declarator), declSpec(std::move(declSpec)),
      declarator(std::move(declarator)) {}

DeclaratorDecl::~DeclaratorDecl() {}

const DeclSpec &DeclaratorDecl::getDeclSpec() const {
  return const_cast<DeclSpec &>(*declSpec);
}

const Declarator &DeclaratorDecl::getDeclarator() const {
  return const_cast<class Declarator &>(*declarator);
}
Declarator::Kind DeclaratorDecl::getKind() const {
  return declarator->getKind();
}

const Type DeclaratorDecl::getType() const {
  return declSpec->getKind() == Type::Struct
             ? Type(static_cast<StructSpec &>(*declSpec).getIdentifier(),
                    declarator->pointer_size())
             : Type(declSpec->getKind(), declarator->pointer_size());
}

FuncDef::FuncDef(Locatable loc, DeclaratorDeclPtr decl,
                 CompStmtPtr compoundStmt)
    : Decl(loc, Kind::FuncDef), decl(std::move(decl)),
      compoundStmt(std::move(compoundStmt)) {}

const class DeclaratorDecl &FuncDef::getDecl() const {
  return const_cast<class DeclaratorDecl &>(*decl);
}

const CompStmt &FuncDef::getCompStmt() const {
  return const_cast<CompStmt &>(*compoundStmt);
}
Symbol FuncDef::getIdentifier() const {
  return static_cast<const FunctionDeclarator &>(getDecl().getDeclarator())
      .getIdentifier();
}

StructDef::StructDef(Locatable loc, Symbol iden,
                     std::vector<DeclaratorDeclPtr> decls)
    : Decl(loc, Decl::StructDef), iden(iden), field_decls(std::move(decls)) {}
// void StructDef::add_field(DeclPtr field) {
//   field_decls.emplace_back(std::move(field));
// }
Symbol StructDef::getIdentifier() const { return iden; }

const std::vector<std::reference_wrapper<const DeclaratorDecl>>
StructDef::getFieldDecls() const {
  std::vector<std::reference_wrapper<const DeclaratorDecl>> res;
  for (auto &structDecl : field_decls)
    res.emplace_back(std::cref(*structDecl));
  return res;
}
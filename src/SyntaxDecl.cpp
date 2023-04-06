#include "SyntaxDecl.hpp"
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxStmt.hpp"
#include <cassert>
#include <utility>
#include <vector>

void TranslationUnit::insert(DeclPtr decl) {
  declarations.push_back(std::move(decl));
}

const std::vector<std::reference_wrapper<const Decl>>
TranslationUnit::getDeclarations() const {
  std::vector<std::reference_wrapper<const Decl>> res;
  for (auto &decl : declarations)
    res.emplace_back(std::cref(*decl));
  return res;
}

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

bool Declarator::is_named() const { return kind == Named; }

bool Declarator::is_valued() const {
  return is_named() &&
         as_named_declarator().getKind() == NamedDeclarator::Valued;
}

bool Declarator::is_variable() const {
  return is_valued() &&
         as_valued_declarator().getKind() == ValuedDeclarator::Variable;
}

bool Declarator::is_function_variable() const {
  return is_valued() &&
         as_valued_declarator().getKind() == ValuedDeclarator::FunctionVariable;
}

bool Declarator::is_function() const {
  return is_named() &&
         as_named_declarator().getKind() == NamedDeclarator::Function;
}

bool Declarator::is_abstract() const { return kind == Abstract; }

const NamedDeclarator &Declarator::as_named_declarator() const {
  assert(is_named());
  return static_cast<const NamedDeclarator &>(*this);
}

const ValuedDeclarator &Declarator::as_valued_declarator() const {
  assert(is_valued());
  return static_cast<const ValuedDeclarator &>(*this);
}
const VariableDeclarator &Declarator::as_variable_declarator() const {
  assert(is_variable());
  return static_cast<const VariableDeclarator &>(*this);
}
const FunctionVariableDeclarator &
Declarator::as_function_variable_declarator() const {
  assert(is_function_variable());
  return static_cast<const FunctionVariableDeclarator &>(*this);
}
const FunctionDeclarator &Declarator::as_function_declarator() const {
  assert(is_function());
  return static_cast<const FunctionDeclarator &>(*this);
}
const AbstractDeclarator &Declarator::as_abstract_declarator() const {
  assert(is_abstract());
  return static_cast<const AbstractDeclarator &>(*this);
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

bool Decl::is_function_definition() const { return kind == FuncDef; }

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

const NamedDeclarator &NamedDeclaratorDecl::getDeclarator() const {
  return DeclaratorDecl::getDeclarator().as_named_declarator();
}

const AbstractDeclarator &AbstractDecl::getDeclarator() const {
  return DeclaratorDecl::getDeclarator().as_abstract_declarator();
}

FunctionDefinition::FunctionDefinition(Locatable loc, DeclaratorDeclPtr decl,
                 CompStmtPtr compoundStmt)
    : Decl(loc, Kind::FuncDef), decl(std::move(decl)),
      compoundStmt(std::move(compoundStmt)) {}

const class DeclaratorDecl &FunctionDefinition::getDecl() const {
  return const_cast<class DeclaratorDecl &>(*decl);
}

const CompStmt &FunctionDefinition::getCompStmt() const {
  return const_cast<CompStmt &>(*compoundStmt);
}

Symbol FunctionDefinition::getIdentifier() const {
  return getDeclarator().getIdentifier();
}

const FunctionDeclarator &FunctionDefinition::getDeclarator() const {
  return getDecl().getDeclarator().as_function_declarator();
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
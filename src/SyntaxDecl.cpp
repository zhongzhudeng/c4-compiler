#include "SyntaxDecl.hpp"
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxStmt.hpp"
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
bool Declarator::is_paren() { return paren; }

ValuedDeclarator::ValuedDeclarator(Locatable loc, Symbol iden, Kind kind)
    : Declarator(loc, Declarator::Valued), iden(iden), kind(kind) {}
ValuedDeclarator::Kind ValuedDeclarator::getKind() const { return kind; }
Symbol ValuedDeclarator::getIdentifier() const { return iden; }

VariableDeclarator::VariableDeclarator(Locatable loc, Symbol iden)
    : ValuedDeclarator(loc, iden, Kind::Variable) {}

FunctionVariableDeclarator::FunctionVariableDeclarator(
    Locatable loc, ValuedDeclaratorPtr decl,
    std::vector<DeclaratorDeclPtr> params)
    : ValuedDeclarator(loc, decl->getIdentifier(), Kind::FunctionVariable),
      params(std::move(params)) {
  for (auto p : decl->getPointers())
    add_pointer(p);
}

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
    : Declarator(loc, Kind::Function), iden(iden), params(std::move(params)) {}

Symbol FunctionDeclarator::getIdentifier() const { return iden; }

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

StructDef::StructDef(Locatable loc, Symbol iden, std::vector<DeclPtr> decls)
    : Decl(loc, Decl::StructDef), iden(iden), field_decls(std::move(decls)) {}

Symbol StructDef::getIdentifier() const { return iden; }

const std::vector<std::reference_wrapper<const Decl>>
StructDef::getFieldDecls() const {
  std::vector<std::reference_wrapper<const Decl>> res;
  for (auto &structDecl : field_decls)
    res.emplace_back(std::cref(*structDecl));
  return res;
}
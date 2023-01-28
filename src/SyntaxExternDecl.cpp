#include "SyntaxExternDecl.hpp"
#include "Diagnostic.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"

ExternDecl::ExternDecl(Locatable loc, Kind kind) : loc(loc), kind(kind) {}
ExternDecl::~ExternDecl() {}
const Locatable &ExternDecl::getLoc() { return loc; }
ExternDecl::Kind ExternDecl::getKind() const { return kind; }

FuncDef::FuncDef(Locatable loc, DeclPtr decl, CompStmtPtr compoundStmt)
    : ExternDecl(loc, Kind::FuncDef), decl(std::move(decl)),
      compoundStmt(std::move(compoundStmt)) {}

const class Decl &FuncDef::getDecl() const {
  return const_cast<class Decl &>(*decl);
}
const CompStmt &FuncDef::getCompStmt() const {
  return const_cast<CompStmt &>(*compoundStmt);
}

Decl::Decl(Locatable loc, DeclSpecPtr declSpec, DeclaratorPtr declarator)
    : ExternDecl(loc, Kind::Decl), declSpec(std::move(declSpec)),
      declarator(std::move(declarator)) {}
Decl::~Decl() {}

const DeclSpec &Decl::getDeclSpec() const {
  return const_cast<DeclSpec &>(*declSpec);
}
bool Decl::isNullDeclarator() const { return declarator == nullptr; }
const Declarator &Decl::getDeclarator() const {
  return const_cast<Declarator &>(*declarator);
}
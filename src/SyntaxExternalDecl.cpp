#include "Diagnostic.hpp"
#include "Printer.hpp"
#include "SyntaxExternalDecl.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"

ExternalDecl::ExternalDecl(Locatable loc, DeclarationPtr Decl) : AST(loc), decl(std::move(Decl)), hasStmt(false) {};
ExternalDecl::ExternalDecl(Locatable loc, DeclarationPtr Decl, StmtPtr Comp) : AST(loc), decl(std::move(Decl)), comp(std::move(Comp)), hasStmt(true) {};
void ExternalDecl::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
const Declaration &ExternalDecl::getDecl() const {return const_cast<Declaration &>(*decl);};
const Stmt &ExternalDecl::getStmt() const {return const_cast<Stmt &>(*comp);};
bool ExternalDecl::hasComp() const {return hasStmt;};

TranslationUnit::TranslationUnit(Locatable loc, std::vector<ExternalDeclPtr> exDecl) : AST(loc), exDecl(std::move(exDecl)) {};
void TranslationUnit::accept(ASTPrinter &visitor) { visitor.Visit(*this); }
const std::vector<ExternalDeclPtr> &TranslationUnit::getExternalDecl() const { return const_cast<std::vector<ExternalDeclPtr> &>(exDecl); };

#include "SyntaxExternalDecl.hpp"
#include "Diagnostic.hpp"
#include "SyntaxStmt.hpp"

ExternDecl::ExternDecl(Locatable loc, Kind kind) : loc(loc), kind(kind) {}
const Locatable &ExternDecl::getLoc() { return loc; }
ExternDecl::Kind ExternDecl::getKind() const { return kind; }

FuncDef::FuncDef(Locatable loc, DeclPtr decl, CompStmtPtr compoundStmt)
    : ExternDecl(loc, Kind::FuncDef), decl(std::move(decl)),
      compoundStmt(std::move(compoundStmt)) {}
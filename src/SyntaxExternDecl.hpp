#pragma once
#include "Diagnostic.hpp"
#include "Syntax.hpp"

class ExternDecl {
public:
  enum Kind { FuncDef, Decl };

private:
  Locatable loc;
  Kind kind;

public:
  ExternDecl(Locatable loc, Kind kind);
  virtual ~ExternDecl();
  const Locatable &getLoc();
  Kind getKind() const;
  void print(std::ostream &os) const;
};

class FuncDef final : public ExternDecl {
  DeclPtr decl;
  CompStmtPtr compoundStmt;

public:
  FuncDef(Locatable loc, DeclPtr decl, CompStmtPtr compoundStmt);
  const class Decl &getDecl() const;
  const CompStmt &getCompStmt() const;
};

class Decl : public ExternDecl {
  DeclSpecPtr declSpec;
  DeclaratorPtr declarator;

public:
  Decl(Locatable loc, DeclSpecPtr declSpec, DeclaratorPtr declarator);
  virtual ~Decl();
  const DeclSpec &getDeclSpec() const;
  bool isNullDeclarator() const;
  const Declarator &getDeclarator() const;
  void print(std::ostream &os, unsigned indentation = 0) const;
};

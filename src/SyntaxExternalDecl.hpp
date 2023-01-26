#pragma once
#include "Syntax.hpp"
#include <vector>

class ExternDecl {
public:
  enum Kind { FuncDef, Decl };

private:
  Locatable loc;
  Kind kind;

public:
  ExternDecl(Locatable loc, Kind kind);
  virtual ~ExternDecl() {}
  const Locatable &getLoc();
  Kind getKind() const;
};

class FuncDef : public ExternDecl {
  DeclPtr decl;
  CompStmtPtr compoundStmt;

public:
  FuncDef(Locatable loc, DeclPtr decl, CompStmtPtr compoundStmt);
  // TODO: getters
};

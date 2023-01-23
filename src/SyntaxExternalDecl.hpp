#pragma once
#include "Syntax.hpp"

class ExternalDecl : public AST {
  DeclarationPtr decl;
  StmtPtr comp;
public:
  ExternalDecl(Locatable loc, DeclarationPtr decl, StmtPtr comp);
  ExternalDecl(Locatable loc, DeclarationPtr decl);
  virtual void accept(ASTPrinter &visitor) override;
  virtual ~ExternalDecl() = default;
  const Declaration &getDecl() const;
  const Stmt &getStmt() const;
};

class TranslationUnit : public AST {
  std::vector<ExternalDeclPtr> exDecl;
public:
  TranslationUnit(Locatable loc, std::vector<ExternalDeclPtr> exDecl);
  virtual void accept(ASTPrinter &visitor) override;
  virtual ~TranslationUnit() = default;
  const std::vector<ExternalDeclPtr> &getExternalDecl() const;
};

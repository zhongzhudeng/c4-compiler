#pragma once

#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxExternalDecl.hpp"
#include <vector>

class Decl : public ExternDecl {
  DeclSpecPtr declSpec;
  DeclaratorPtr declarator;

public:
  Decl(Locatable loc, DeclSpecPtr declSpec, DeclaratorPtr declarator);
  const DeclSpec &getDeclSpec() const;
  bool isNullDeclarator() const;
  const Declarator &getDeclarator() const;
  void print(std::ostream &os, unsigned indentation = 0) const;
};

class DeclSpec {
public:
  enum Kind { Void, Char, Int, Struct };

private:
  Locatable loc;
  Kind kind;

public:
  DeclSpec(Locatable loc, Kind kind);
  virtual ~DeclSpec() {}
  const Locatable &getLoc();
  Kind getKind() const;
};

class VoidSpec final : public DeclSpec {
public:
  VoidSpec(Locatable loc);
};
class CharSpec final : public DeclSpec {
public:
  CharSpec(Locatable loc);
};
class IntSpec final : public DeclSpec {
public:
  IntSpec(Locatable loc);
};

class StructSpec final : public DeclSpec {
  Symbol iden;
  std::vector<DeclPtr> decls;
  bool is_bracketed;

public:
  StructSpec(Locatable loc, Symbol iden, bool is_bracketed,
             std::vector<DeclPtr> decls);
  bool isAnoymous() const;
  Symbol getIdentifier() const;
  bool isBracketed() const;
  const std::vector<std::reference_wrapper<const Decl>> getDecls() const;
};

class Declarator {
public:
  enum Kind { Primitive, Pointer, Function };

private:
  Locatable loc;
  Kind kind;

public:
  Declarator(Locatable loc, Kind kind);
  virtual ~Declarator() {}
  const Locatable &getLoc();
  Kind getKind() const;
};

class PrimitiveDeclarator final : public Declarator {
  Symbol iden;

public:
  PrimitiveDeclarator(Locatable loc, Symbol iden);
  Symbol getIdentifier() const;
};
class PointerDeclarator final : public Declarator {
  DeclaratorPtr decl;

public:
  PointerDeclarator(Locatable loc, DeclaratorPtr decl);
  const Declarator &getDeclarator() const;
};
class FunctionDeclarator final : public Declarator {
  DeclaratorPtr decl;
  std::vector<DeclPtr> params;

public:
  FunctionDeclarator(Locatable loc, DeclaratorPtr decl,
                     std::vector<DeclPtr> params);
  const Declarator &getDeclarator() const;
  const std::vector<std::reference_wrapper<const Decl>> getParams() const;
};
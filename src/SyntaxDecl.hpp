#pragma once

#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "Type.hpp"
#include <cstddef>
#include <vector>

class DeclSpec {
public:
private:
  Locatable loc;
  Type::Kind kind;

public:
  DeclSpec(Locatable loc, Type::Kind kind);
  virtual ~DeclSpec();
  const Locatable &getLoc();
  Type::Kind getKind() const;
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
  const StructDef &ref;
  StructDefPtr struct_def;

public:
  StructSpec(Locatable loc, const StructDef &ref);
  StructSpec(Locatable loc, StructDefPtr struct_def);
  Symbol getIdentifier() const;
  bool has_def() const;
  const StructDef &getDef() const;
};

class Declarator {
public:
  enum Kind { Valued, Function, Abstract };

private:
  Locatable loc;
  Kind kind;
  std::vector<Locatable> pointers;
  bool paren;

public:
  Declarator(Locatable loc, Kind kind);
  Declarator(DeclaratorPtr decl, Kind kind);
  virtual ~Declarator();
  const Locatable &getLoc() const;
  Kind getKind() const;
  const std::vector<Locatable> &getPointers() const;
  size_t pointer_size() const;
  void add_pointer(Locatable loc);
  void set_paren();
  bool is_paren();
};

class ValuedDeclarator : public Declarator {
public:
  enum Kind { Variable, FunctionVariable };

private:
  Symbol iden;
  Kind kind;

public:
  ValuedDeclarator(Locatable loc, Symbol iden, Kind kind);
  Kind getKind() const;
  Symbol getIdentifier() const;
};

class VariableDeclarator final : public ValuedDeclarator {
public:
  VariableDeclarator(Locatable loc, Symbol iden);
};

class FunctionVariableDeclarator final : public ValuedDeclarator {
  std::vector<DeclaratorDeclPtr> params;
  std::vector<Locatable> return_pointers;

public:
  FunctionVariableDeclarator(Locatable loc, ValuedDeclaratorPtr decl,
                             std::vector<DeclaratorDeclPtr> params);
  size_t return_pointer_size() const;
  void add_return_pointer(Locatable loc);
  const std::vector<std::reference_wrapper<const DeclaratorDecl>>
  getParams() const;
};

class FunctionDeclarator final : public Declarator {
  Symbol iden;
  std::vector<DeclaratorDeclPtr> params;

public:
  FunctionDeclarator(Locatable loc, Symbol iden,
                     std::vector<DeclaratorDeclPtr> params);
  Symbol getIdentifier() const;
  const std::vector<std::reference_wrapper<const DeclaratorDecl>>
  getParams() const;
};

class AbstractDeclarator final : public Declarator {
public:
  AbstractDeclarator(Locatable loc);
};

class Decl {
public:
  enum Kind { Declarator, LabelDecl, FuncDef, StructDef };

private:
  Locatable loc;
  Kind kind;

public:
  Decl(Locatable loc, Kind kind);
  virtual ~Decl();
  const Locatable &getLoc() const;
  Kind getKind() const;
  void print(std::ostream &os, unsigned indentation = 0) const;
};

class DeclaratorDecl : public Decl {
private:
  DeclSpecPtr declSpec;
  DeclaratorPtr declarator;

public:
  DeclaratorDecl(Locatable loc, DeclSpecPtr declSpec, DeclaratorPtr declarator);
  virtual ~DeclaratorDecl();
  const DeclSpec &getDeclSpec() const;
  const class Declarator &getDeclarator() const;
  Declarator::Kind getKind() const;
  const Type getType() const;
};

class LabelDecl final : public Decl {
  Symbol iden;

public:
  LabelDecl(Locatable loc, Symbol iden);
  Symbol getIdentifier() const;
};

class FuncDef final : public Decl {
  DeclaratorDeclPtr decl;
  CompStmtPtr compoundStmt;

public:
  FuncDef(Locatable loc, DeclaratorDeclPtr decl, CompStmtPtr compoundStmt);
  const DeclaratorDecl &getDecl() const;
  const CompStmt &getCompStmt() const;
  Symbol getIdentifier() const;
};

class StructDef final : public Decl {
  Symbol iden;
  std::vector<DeclPtr> field_decls;

public:
  StructDef(Locatable loc, Symbol iden, std::vector<DeclPtr> decls);
  Symbol getIdentifier() const;
  const std::vector<std::reference_wrapper<const Decl>> getFieldDecls() const;
};
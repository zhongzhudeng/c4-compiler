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
  const StructDefinition &ref;

public:
  StructSpec(Locatable loc, const StructDefinition &ref);
};

class Declarator {
public:
  enum Kind { Valued, Function };

private:
  Locatable loc;
  Symbol iden;
  Kind kind;
  std::vector<Locatable> pointers;
  bool paren;

public:
  Declarator(Locatable loc, Kind kind);
  Declarator(Locatable loc, Kind kind, std::vector<Locatable> pointers);
  Declarator(DeclaratorPtr decl, Kind kind);
  virtual ~Declarator();
  const Locatable &getLoc() const;
  Kind getKind() const;
  size_t pointer_size() const;
  void add_pointer(Locatable loc);
  void set_paren();
  bool is_paren() const;
  bool is_abstract() const;

  bool is_named() const;
  bool is_valued() const;
  bool is_variable() const;
  bool is_function_variable() const;
  bool is_function() const;
};

class ValuedDeclarator : public Declarator {
public:
  enum Kind { Variable, FunctionVariable };

private:
  Kind kind;

public:
  ValuedDeclarator(Locatable loc, Symbol iden, Kind kind);
  ValuedDeclarator(Locatable loc, Symbol iden, Kind kind,
                   std::vector<Locatable> pointers);
  virtual ~ValuedDeclarator();
  Kind getKind() const;
};

class VariableDeclarator final : public ValuedDeclarator {
public:
  VariableDeclarator(Locatable loc, Symbol iden);
};

class FunctionVariableDeclarator final : public ValuedDeclarator {
  std::vector<DeclaratorDeclPtr> params;
  std::vector<Locatable> return_pointers;

public:
  FunctionVariableDeclarator(Locatable loc, VariableDeclaratorPtr decl,
                             std::vector<DeclaratorDeclPtr> params);
  size_t return_pointer_size() const;
  void add_return_pointer(Locatable loc);
};

class FunctionDeclarator final : public Declarator {
  std::vector<ValuedDeclPtr> params;

public:
  FunctionDeclarator(Locatable loc, Symbol iden,
                     std::vector<ValuedDeclPtr> params);
};

class TranslationUnit {
private:
  std::vector<DeclPtr> declarations;

public:
  void insert(DeclPtr decl);
  void print(std::ostream &os) const;
};

class Decl {
public:
  enum Kind { DeclaratorDecl, FunctionDefinition, StructDefinition };

private:
  Locatable loc;
  Kind kind;

public:
  Decl(Locatable loc, Kind kind);
  virtual ~Decl();
  const Locatable &location() const;
  void print(std::ostream &os, unsigned indentation = 0) const;
};

class DeclaratorDecl : public Decl {
private:
  DeclSpecPtr declSpec;
  DeclaratorPtr declarator;

public:
  DeclaratorDecl(Locatable loc, DeclSpecPtr declSpec, DeclaratorPtr declarator);
};

class ValuedDecl : public DeclaratorDecl {

public:
  ValuedDecl(Locatable loc, DeclSpecPtr declSpec,
             ValuedDeclaratorPtr declarator);
};

class VariableDecl final : public ValuedDecl {
public:
  VariableDecl(Locatable loc, DeclSpecPtr declSpec,
               VariableDeclaratorPtr declarator);
};

class FunctionVariableDecl final : public ValuedDecl {
public:
  FunctionVariableDecl(Locatable loc, DeclSpecPtr declSpec,
                       FunctionVariableDeclaratorPtr declarator);
};

class FunctionDecl final : public DeclaratorDecl {
public:
  FunctionDecl(Locatable loc, DeclSpecPtr declSpec,
               FunctionDeclaratorPtr declarator);
};

class StructDefinition final : public Decl {
  Symbol iden_;
  std::vector<ValuedDeclPtr> field_decls;

public:
  StructDefinition(Locatable loc, Symbol iden,
                   std::vector<ValuedDeclPtr> decls);
  Symbol iden() const;
};

class FunctionDefinition final : public Decl {
  FunctionDeclPtr decl;
  CompStmtPtr compoundStmt;

public:
  FunctionDefinition(Locatable loc, FunctionDeclPtr decl,
                     CompStmtPtr compoundStmt);
};

class LabelDecl final {
  Symbol iden_;

public:
  LabelDecl(Locatable loc, Symbol iden);
  Symbol iden() const;
};
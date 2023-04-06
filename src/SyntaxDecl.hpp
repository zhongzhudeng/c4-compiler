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
  enum Kind { Named, Abstract };

private:
  Locatable loc;
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
  const std::vector<Locatable> &getPointers() const;
  size_t pointer_size() const;
  void add_pointer(Locatable loc);
  void set_paren();
  bool is_paren() const;
  bool is_named() const;
  bool is_valued() const;
  bool is_variable() const;
  bool is_function_variable() const;
  bool is_function() const;
  bool is_abstract() const;
  const NamedDeclarator &as_named_declarator() const;
  const ValuedDeclarator &as_valued_declarator() const;
  const VariableDeclarator &as_variable_declarator() const;
  const FunctionVariableDeclarator &as_function_variable_declarator() const;
  const FunctionDeclarator &as_function_declarator() const;
  const AbstractDeclarator &as_abstract_declarator() const;
};

class NamedDeclarator : public Declarator {
public:
  enum Kind { Valued, Function };

private:
  Symbol iden;
  Kind kind;

public:
  NamedDeclarator(Locatable loc, Symbol iden, Kind kind);
  NamedDeclarator(Locatable loc, Symbol iden, Kind kind,
                  std::vector<Locatable> pointers);
  virtual ~NamedDeclarator();
  Kind getKind() const;
  Symbol getIdentifier() const;
};

class ValuedDeclarator : public NamedDeclarator {
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
  FunctionVariableDeclarator(const FunctionVariableDeclarator &other) = delete;
  size_t return_pointer_size() const;
  void add_return_pointer(Locatable loc);
  const std::vector<std::reference_wrapper<const DeclaratorDecl>>
  getParams() const;
};

class FunctionDeclarator final : public NamedDeclarator {
  std::vector<DeclaratorDeclPtr> params;

public:
  FunctionDeclarator(Locatable loc, Symbol iden,
                     std::vector<DeclaratorDeclPtr> params);
  FunctionDeclarator(const FunctionDeclarator &other) = delete;
  const std::vector<std::reference_wrapper<const DeclaratorDecl>>
  getParams() const;
};

class AbstractDeclarator : public Declarator {
public:
  enum Kind { Variable, FunctionVariable };

private:
  Kind kind;

public:
  AbstractDeclarator(Locatable loc);
};

class AbstractVariableDeclarator final : public AbstractDeclarator {};
class AbstractFunctionVariableDeclarator final : public AbstractDeclarator {};

class TranslationUnit {
private:
  std::vector<DeclPtr> declarations;

public:
  void insert(DeclPtr decl);
  const std::vector<std::reference_wrapper<const Decl>> getDeclarations() const;
  void print(std::ostream &os) const;
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
  bool is_function_definition() const;
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

class NamedDeclaratorDecl : public DeclaratorDecl {
public:
  NamedDeclaratorDecl(Locatable loc, DeclSpecPtr declSpec,
                      NamedDeclaratorPtr declarator);
  Symbol getIdentifier() const;
};

class VariableDecl;
class FunctionVariableDecl;

class ValuedDecl : public NamedDeclaratorDecl {
public:
  ValuedDecl(Locatable loc, DeclSpecPtr declSpec,
             ValuedDeclaratorPtr declarator);
  ValuedDeclarator::Kind getKind() const;
  const VariableDecl &as_variable() const;
  const FunctionVariableDecl &as_function_variable() const;
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

class FunctionDecl final : public NamedDeclaratorDecl {
public:
  FunctionDecl(Locatable loc, DeclSpecPtr declSpec,
               FunctionDeclaratorPtr declarator);
};

class AbstractDecl : public DeclaratorDecl {};

class AbstractVariableDecl final : public AbstractDecl {};

class AbstractFunctionVariableDecl final : public AbstractDecl {};

class LabelDecl final : public Decl {
  Symbol iden;

public:
  LabelDecl(Locatable loc, Symbol iden);
  Symbol getIdentifier() const;
};

class FunctionDefinition final : public Decl {
  DeclaratorDeclPtr decl;
  CompStmtPtr compoundStmt;

public:
  FunctionDefinition(Locatable loc, DeclaratorDeclPtr decl,
                     CompStmtPtr compoundStmt);
  const DeclaratorDecl &getDecl() const;
  const FunctionDeclarator &getDeclarator() const;
  const CompStmt &getCompStmt() const;
  Symbol getIdentifier() const;
};

class StructDef final : public Decl {
  Symbol iden;
  std::vector<DeclaratorDeclPtr> field_decls;

public:
  StructDef(Locatable loc, Symbol iden, std::vector<DeclaratorDeclPtr> decls);
  StructDef(const StructDef &other) = delete;
  Symbol getIdentifier() const;
  const std::vector<std::reference_wrapper<const DeclaratorDecl>>
  getFieldDecls() const;
};
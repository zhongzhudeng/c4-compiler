#pragma once

// Used by the type checker
// Maps names to Variable Declarations
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include <optional>
#include <unordered_map>
#include <vector>

class Scope;
class FuncScope;
class ProgramScope;

class Scope {
public:
  Scope(const Scope *parent = nullptr);
  std::optional<std::reference_wrapper<DeclaratorDecl>>
  find_variable(Symbol iden) const;
  std::optional<std::reference_wrapper<DeclaratorDecl>>
  find_function_variable(Symbol iden) const;
  std::optional<std::reference_wrapper<StructDef>>
  find_struct(Symbol iden) const;
  void insert_variable(DeclaratorDecl &variable);
  // TODO: solve overload?
  void insert_function_variable(DeclaratorDecl &function_variable);
  void insert_struct(StructDef &struct_def);

private:
  std::unordered_map<Symbol, std::reference_wrapper<DeclaratorDecl>> variables;
  std::unordered_map<Symbol, std::reference_wrapper<DeclaratorDecl>>
      function_variables;
  std::unordered_map<Symbol, std::reference_wrapper<StructDef>> structs;
  const Scope *parent;
};

class FuncScope : public Scope {
public:
  FuncScope(const ProgramScope *parent);
  std::optional<std::reference_wrapper<LabelDecl>>
  find_label(Symbol iden) const;
  void insert_label(LabelDecl &label_decl);
  void push_on_back_list(GoToStmt &stmt, Symbol label);
  const std::vector<std::tuple<std::reference_wrapper<GoToStmt>, Symbol>> &
  get_back_list() const;

private:
  std::vector<std::tuple<std::reference_wrapper<GoToStmt>, Symbol>> back_list;
  std::unordered_map<Symbol, std::reference_wrapper<LabelDecl>> labels;
};

class ProgramScope : public Scope {
public:
  ProgramScope();

  void insert_function(DeclaratorDecl &function);
  void insert_function_definition(FuncDef &functioin_definition);

private:
  std::unordered_map<Symbol, std::reference_wrapper<DeclaratorDecl>> functions;
  std::unordered_map<Symbol, std::reference_wrapper<FuncDef>> function_definitions;
};
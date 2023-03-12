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
  std::optional<std::reference_wrapper<NamedDeclaratorDecl>> find(Symbol iden) const;
  void insert(Decl &decl);
  std::optional<std::reference_wrapper<StructDef>>
  find_struct(Symbol iden) const;
  void insert_struct(StructDef &struct_def);

private:
  std::unordered_map<Symbol, std::reference_wrapper<NamedDeclaratorDecl>> table;
  std::unordered_map<Symbol, std::reference_wrapper<StructDef>> structs;
  const Scope *parent;
};

class FuncScope : public Scope {
public:
  FuncScope(const ProgramScope *parent);
  void insert_parameter(DeclaratorDecl &parameter);
  std::optional<std::reference_wrapper<LabelDecl>>
  find_label(const Symbol iden) const;
  void insert_label(LabelDecl &label_decl);
  void push_on_back_list(GoToStmt &stmt, const Symbol label);
  const std::vector<std::tuple<std::reference_wrapper<GoToStmt>, Symbol>> &
  get_back_list() const;

private:
  std::vector<std::tuple<std::reference_wrapper<GoToStmt>, Symbol>> back_list;
  std::unordered_map<Symbol, std::reference_wrapper<LabelDecl>> labels;
};

class ProgramScope : public Scope {
public:
  ProgramScope();
  std::optional<std::reference_wrapper<FuncDef>> find_function(Symbol iden);
  // std::optional<std::reference_wrapper<DeclaratorDecl>> find_function_decl(Symbol iden);
  void insert_function(FuncDef &function);
  void insert_function_decl(DeclaratorDecl &function_decl);

private:
  std::unordered_map<Symbol, std::reference_wrapper<FuncDef>> functions;
  std::unordered_map<Symbol,
                     std::vector<std::reference_wrapper<DeclaratorDecl>>>
      function_declars;
};

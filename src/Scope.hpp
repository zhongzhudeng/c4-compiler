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
  std::optional<std::reference_wrapper<VarDecl>> find(Symbol iden) const;
  bool insert(VarDecl &varDecl);

private:
  std::unordered_map<Symbol, std::reference_wrapper<VarDecl>> var;
  const Scope *parent;
};

class FuncScope : public Scope {
public:
  FuncScope(const ProgramScope *parent);
  std::optional<std::reference_wrapper<LabelDecl>>
  find_label(Symbol iden) const;
  void insert_label(LabelDecl &label_decl);
  void push_on_back_list(GoToStmt &stmt, Symbol label);

  const std::vector<std::tuple<GoToStmt &, Symbol>> &get_back_list() const;

private:
  std::vector<std::tuple<GoToStmt &, Symbol>> back_list;
  std::unordered_map<Symbol, std::reference_wrapper<LabelDecl>> label_table;
};

class ProgramScope : public Scope {
public:
  ProgramScope();

private:
  std::unordered_map<Symbol, std::reference_wrapper<FuncDecl>> label_table;
};
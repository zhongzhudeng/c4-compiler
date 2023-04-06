#pragma once

// Used by the type checker
// Maps names to Variable Declarations
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include <optional>
#include <unordered_map>
#include <vector>

class Scope;
class FuncScope;
class ProgramScope;

class Scope {
  using VariableDeclfRef = std::reference_wrapper<const VariableDecl>;
  using FunctionVariableDeclRef =
      std::reference_wrapper<const FunctionVariableDecl>;
  using StructDefRef = std::reference_wrapper<const StructDef>;

public:
  Scope(const Scope *parent = nullptr);
  std::optional<VariableDeclfRef> find_variable(Symbol iden) const;
  std::optional<FunctionVariableDeclRef>
  find_function_variable(Symbol iden) const;
  std::optional<StructDefRef> find_struct(Symbol iden) const;
  void insert_variable(const VariableDecl &variable);
  void insert_function_variable(const FunctionVariableDecl &function_variable);
  void insert_struct(const StructDef &struct_def);

private:
  std::unordered_map<Symbol, std::reference_wrapper<const VariableDecl>>
      variables;
  std::unordered_map<Symbol, std::reference_wrapper<const FunctionVariableDecl>>
      function_variables;
  std::unordered_map<Symbol, std::reference_wrapper<const StructDef>> structs;
  const Scope *parent;
};

class FuncScope : public Scope {
  using LabelDeclRef = std::reference_wrapper<const LabelDecl>;

public:
  FuncScope(const ProgramScope *parent);
  std::optional<LabelDeclRef> find_label(const Symbol iden) const;
  void insert_parameter(const ValuedDecl &parameter);
  void insert_label(const LabelDecl &label_decl);
  void push_on_back_list(const GoToStmt &stmt, const Symbol label);
  const std::vector<
      std::tuple<std::reference_wrapper<const GoToStmt>, Symbol>> &
  get_back_list() const;

private:
  std::unordered_map<Symbol, LabelDeclRef> labels;
  std::vector<std::tuple<std::reference_wrapper<const GoToStmt>, Symbol>>
      back_list;
};

class ProgramScope : public Scope {
  using FuncDefRef = std::reference_wrapper<const FunctionDefinition>;
  using FuncDeclRef = std::reference_wrapper<const FunctionDecl>;

public:
  ProgramScope();
  std::optional<FuncDefRef> find_function_definition(Symbol iden);
  std::optional<FuncDeclRef> find_function_decl(Symbol iden);
  void insert_function_definition(const FunctionDefinition &function);
  void insert_function_decl(const FunctionDecl &function_decl);

private:
  std::unordered_map<Symbol, FuncDefRef> function_definitions;
  std::unordered_map<Symbol, std::vector<FuncDeclRef>> function_decls;
};

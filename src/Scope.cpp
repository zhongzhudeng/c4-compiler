#include "Scope.hpp"
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"

#include <cassert>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

Scope::Scope(const Scope *parent) : parent(parent) {}

std::optional<std::reference_wrapper<const VariableDecl>>
Scope::find_variable(Symbol iden) const {
  auto it = variables.find(iden);
  return it != variables.end() ? std::optional<VariableDeclfRef>{it->second}
         : parent              ? parent->find_variable(iden)
                               : std::nullopt;
}

std::optional<std::reference_wrapper<const FunctionVariableDecl>>
Scope::find_function_variable(Symbol iden) const {
  auto it = function_variables.find(iden);
  return it != function_variables.end()
             ? std::optional<FunctionVariableDeclRef>{it->second}
         : parent ? parent->find_function_variable(iden)
                  : std::nullopt;
}

std::optional<std::reference_wrapper<const StructDef>>
Scope::find_struct(Symbol iden) const {
  auto it = structs.find(iden);
  return it != structs.end() ? std::optional<StructDefRef>{it->second}
         : parent            ? parent->find_struct(iden)
                             : std::nullopt;
}

void Scope::insert_variable(const VariableDecl &variable) {
  variables.insert({variable.getIdentifier(), variable});
}

void Scope::insert_function_variable(
    const FunctionVariableDecl &function_variable) {
  function_variables.insert(
      {function_variable.getIdentifier(), function_variable});
}

void Scope::insert_struct(const StructDef &struct_def) {
  structs.insert({struct_def.getIdentifier(), struct_def});
}

FuncScope::FuncScope(const ProgramScope *parent) : Scope(parent) {}

void FuncScope::insert_parameter(const ValuedDecl &parameter) {
  switch (parameter.getKind()) {
  case ValuedDeclarator::Kind::Variable:
    insert_variable(parameter.as_variable());
  case ValuedDeclarator::Kind::FunctionVariable:
    insert_function_variable(parameter.as_function_variable());
  }
}

std::optional<std::reference_wrapper<const LabelDecl>>
FuncScope::find_label(Symbol iden) const {
  auto it = labels.find(iden);
  return it != labels.end()
             ? std::optional<
                   std::reference_wrapper<const LabelDecl>>{it->second}
             : std::nullopt;
}
void FuncScope::insert_label(const LabelDecl &label_decl) {
  labels.insert({label_decl.getIdentifier(), label_decl});
}
void FuncScope::push_on_back_list(const GoToStmt &stmt, Symbol label) {
  back_list.emplace_back(stmt, label);
}
const std::vector<std::tuple<std::reference_wrapper<const GoToStmt>, Symbol>> &
FuncScope::get_back_list() const {
  return back_list;
}

ProgramScope::ProgramScope() : Scope() {}

std::optional<std::reference_wrapper<const FunctionDefinition>>
ProgramScope::find_function_definition(Symbol iden) {
  auto it = function_definitions.find(iden);
  return it != function_definitions.end()
             ? std::optional<FuncDefRef>{it->second}
             : std::nullopt;
}

std::optional<std::reference_wrapper<const FunctionDecl>>
ProgramScope::find_function_decl(Symbol iden) {
  // TODO
}

void ProgramScope::insert_function_definition(
    const FunctionDefinition &function) {
  function_definitions.insert({function.getIdentifier(), function});
}

void ProgramScope::insert_function_decl(const FunctionDecl &function_decl) {
  Symbol iden = function_decl.getIdentifier();
  auto it = function_decls.find(iden);
  if (it != function_decls.end())
    it->second.push_back(function_decl);
  else
    function_decls.insert({iden, std::vector<FuncDeclRef>{function_decl}});
}
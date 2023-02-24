#include "Scope.hpp"
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"

#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

Scope::Scope(const Scope *parent) : parent(parent) {}

std::optional<std::reference_wrapper<DeclaratorDecl>>
Scope::find_variable(Symbol iden) const {
  auto it = variables.find(iden);
  if (it != variables.end())
    return std::optional<std::reference_wrapper<DeclaratorDecl>>{it->second};
  else
    return parent ? parent->find_variable(iden) : std::nullopt;
}

std::optional<std::reference_wrapper<StructDef>>
Scope::find_struct(Symbol iden) const {
  auto it = structs.find(iden);
  if (it != structs.end())
    return std::optional<std::reference_wrapper<StructDef>>{it->second};
  else
    return parent ? parent->find_struct(iden) : std::nullopt;
}

void Scope::insert_variable(DeclaratorDecl &variable) {
  variables.insert(
      {static_cast<const VariableDeclarator &>(variable.getDeclarator())
           .getIdentifier(),
       variable});
}
void Scope::insert_function_variable(DeclaratorDecl &function_variable) {
  function_variables.insert({static_cast<const FunctionVariableDeclarator &>(
                                 function_variable.getDeclarator())
                                 .getIdentifier(),
                             function_variable});
}
void Scope::insert_struct(StructDef &struct_def) {
  structs.insert({struct_def.getIdentifier(), struct_def});
}

FuncScope::FuncScope(const ProgramScope *parent) : Scope(parent) {}
std::optional<std::reference_wrapper<LabelDecl>>
FuncScope::find_label(Symbol iden) const {
  auto it = labels.find(iden);
  return it != labels.end()
             ? std::optional<std::reference_wrapper<LabelDecl>>{it->second}
             : std::nullopt;
}
void FuncScope::insert_label(LabelDecl &label_decl) {
  labels.insert({label_decl.getIdentifier(), label_decl});
}
void FuncScope::push_on_back_list(GoToStmt &stmt, Symbol label) {
  back_list.emplace_back(stmt, label);
}
const std::vector<std::tuple<std::reference_wrapper<GoToStmt>, Symbol>> &
FuncScope::get_back_list() const {
  return back_list;
}

ProgramScope::ProgramScope() : Scope() {}

void ProgramScope::insert_function(DeclaratorDecl &function) {
  functions.insert(
      {static_cast<const FunctionDeclarator &>(function.getDeclarator())
           .getIdentifier(),
       function});
}

void ProgramScope::insert_function_definition(FuncDef &functioin_definition) {
  function_definitions.insert(
      {functioin_definition.getIdentifier(), functioin_definition});
}
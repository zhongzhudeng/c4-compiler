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

std::optional<std::reference_wrapper<NamedDeclaratorDecl>>
Scope::find(Symbol iden) const {
  auto it = table.find(iden);
  return it == table.end()
             ? std::nullopt
             : std::optional<std::reference_wrapper<NamedDeclaratorDecl>>{
                   it->second};
}

// TODO
void Scope::insert(Decl &decl){}

// std::optional<std::reference_wrapper<DeclaratorDecl>>
// Scope::find_variable(Symbol iden) const {
//   auto it = variables.find(iden);
//   if (it != variables.end())
//     return std::optional<std::reference_wrapper<DeclaratorDecl>>{it->second};
//   else
//     return parent ? parent->find_variable(iden) : std::nullopt;
// }

std::optional<std::reference_wrapper<StructDef>>
Scope::find_struct(Symbol iden) const {
  auto it = structs.find(iden);
  if (it != structs.end())
    return std::optional<std::reference_wrapper<StructDef>>{it->second};
  else
    return parent ? parent->find_struct(iden) : std::nullopt;
}

// void Scope::insert_value(DeclaratorDecl &value) {
//   auto declarator = value.getDeclarator();
//   assert(declarator.getKind() == Declarator::Valued);
//   switch (static_cast<const ValuedDeclarator &>(declarator).getKind()) {
//   case ValuedDeclarator::Variable:
//     variables.insert(
//         {static_cast<const VariableDeclarator &>(value.getDeclarator())
//              .getIdentifier(),
//          value});
//   case ValuedDeclarator::FunctionVariable:
//     // TODO: solve overload?
//     function_variables.insert(
//         {static_cast<const FunctionVariableDeclarator
//         &>(value.getDeclarator())
//              .getIdentifier(),
//          value});
//   }
// }

void Scope::insert_struct(StructDef &struct_def) {
  structs.insert({struct_def.getIdentifier(), struct_def});
}

FuncScope::FuncScope(const ProgramScope *parent) : Scope(parent) {}
// void FuncScope::insert_parameter(DeclaratorDecl &parameter) {
//   auto decl = parameter.getDeclarator();
//   switch (decl.getKind()) {
//   case Declarator::Valued: {
//     auto valued_declarator = static_cast<ValuedDeclarator &>(decl);
//     switch (valued_declarator.getKind()) {
//     case ValuedDeclarator::Variable: {
//       if (find_variable(valued_declarator.getIdentifier()).has_value())
//         errorloc(valued_declarator.getLoc(),
//                  "variable already defined in this scope");
//       insert_value(parameter);
//       break;
//     }
//     case ValuedDeclarator::FunctionVariable:
//       // auto function_variable = static_cast<FunctionVariableDeclarator
//       // &>(decl);
//       // TODO: find the function variable name
//       insert_function_variable(parameter);
//       break;
//     }
//     break;
//   }
//   case Declarator::Function:
//     assert(false);
//     break;
//   case Declarator::Abstract:
//     break;
//   }
// }

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

std::optional<std::reference_wrapper<FuncDef>>
ProgramScope::find_function(Symbol iden) {}

void ProgramScope::insert_function(FuncDef &function) {
  // functions.insert(
  //     {static_cast<const FunctionDeclarator &>(function.getDeclarator())
  //          .getIdentifier(),
  //      function});
}

void ProgramScope::insert_function_decl(DeclaratorDecl &function_decl) {}

// void ProgramScope::insert_function_definition(FuncDef &functioin_definition)
// {
//   functions.insert(
//       {functioin_definition.getIdentifier(), functioin_definition});
// }
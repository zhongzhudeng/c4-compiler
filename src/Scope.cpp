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

std::optional<std::reference_wrapper<VarDecl>> Scope::find(Symbol iden) const {
  auto it = var.find(iden);
  if (it != var.end())
    return std::optional<std::reference_wrapper<VarDecl>>{it->second};
  else
    return parent ? parent->find(iden) : std::nullopt;
}

bool Scope::insert(VarDecl &varDecl) {
  return var.insert({varDecl.getIdentifier(), varDecl}).second;
}

FuncScope::FuncScope(const ProgramScope *parent) : Scope(parent) {}
void FuncScope::insert_label(LabelDecl &label_decl) {
  label_table.insert({label_decl.getIdentifier(), label_decl});
}
void FuncScope::push_on_back_list(GoToStmt &stmt, Symbol label) {
  back_list.emplace_back(stmt, label);
}
const std::vector<std::tuple<GoToStmt &, Symbol>> &
FuncScope::get_back_list() const {
  return back_list;
}
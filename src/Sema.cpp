#include "Sema.hpp"
#include "Diagnostic.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"
#include "Token.hpp"
#include <functional>

Sema::Sema() {}
Sema::~Sema() {}

// Label names shall be unique within a function.
LabeledStmtPtr Sema::ActOnLabeledStmt(Locatable loc, LabelDeclPtr label_decl,
                                      StmtPtr stmt) {
  // push the LabelDecl * into function scope.
  // if within this function containes this label, error.
  if (cur_func_scope->find_label(label_decl->getIdentifier()).has_value())
    errorloc(loc, "Redefinition of label '", label_decl->getIdentifier(), "'");
  // insert the label_decl into current function scope.
  cur_func_scope->insert_label(*label_decl);
  return std::make_unique<LabeledStmt>(loc, std::move(label_decl),
                                       std::move(stmt));
}

GoToStmtPtr Sema::ActOnGotoStmt(Locatable loc, Symbol iden) {
  // if within this function containes this label
  auto label = cur_func_scope->find_label(iden);
  if (label.has_value())
    // make the stmt, Label refers to the LabelDecl.
    return std::make_unique<GoToStmt>(loc, &label.value().get());
  // push the Label into backing list
  auto stmt = std::make_unique<GoToStmt>(loc);
  cur_func_scope->push_on_back_list(*stmt, iden);
  return stmt;
}

// When finish parsing a function, solve the backing list
void Sema::resolve_backing_label_decl() {
  // find the LabelDecl via label, set GoToStmt's label
  for (auto [stmt, iden] : cur_func_scope->get_back_list()) {
    auto label_decl = cur_func_scope->find_label(iden);
    if (label_decl.has_value())
      stmt.set_label_decl(&label_decl.value().get());
    else
      errorloc(stmt.getLoc(), "Use of undeclared label `", iden, '`');
  }
}
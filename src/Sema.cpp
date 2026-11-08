#include "Sema.hpp"
#include "Diagnostic.hpp"
#include "Scope.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxStmt.hpp"
#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <utility>

Sema::Sema() {
  program_scope = new ProgramScope();
  cur_scope = program_scope;
}
Sema::~Sema() { delete program_scope; }

IdentifierExprPtr Sema::ActOnIdenExpr(Locatable loc, Symbol iden) {
  auto decl = cur_scope->find_variable(iden);
  if (!decl.has_value())
    errorloc(loc, "Undefined symbol ", *iden);
  return std::make_unique<IdentifierExpr>(loc, decl->get());
}

ArraySubscriptExprPtr Sema::ActOnArraySubscriptExpr(Locatable loc, ExprPtr expr,
                                                    ExprPtr squared_expr) {
  // Type ty = ;
  // return std::make_unique<ArraySubscriptExpr>(loc, std::move(expr),
  //                                             std::move(squared_expr), ty);
}

DeclaratorPtr Sema::ActOnNonFuncDeclarator(DeclaratorPtr decl) {
  switch (decl->getKind()) {
  case Declarator::Named: {
    Symbol iden = static_cast<const NamedDeclarator &>(*decl).getIdentifier();
    if (cur_scope->find_variable(iden).has_value())
      errorloc(decl->getLoc(), "Redefinition of symbol", iden);
    return decl;
  }
  case Declarator::Abstract:
    return decl;
  }
}

FunctionDefinitionPtr Sema::ActOnFunctionDefinition(Locatable loc, DeclaratorDeclPtr decl,
                                         CompStmtPtr stmt) {
  assert(cur_scope == program_scope);
  Symbol iden = decl->getDeclarator().as_function_declarator().getIdentifier();
  if (program_scope->find_function_definition(iden))
    errorloc(loc, "Redefinition of function ", *iden);
  auto function =
      std::make_unique<FunctionDefinition>(loc, std::move(decl), std::move(stmt));
  program_scope->insert_function_definition(*function);
  return function;
}

DeclaratorDeclPtr Sema::ActOnFunctionDecl(DeclaratorDeclPtr decl) {
  assert(cur_scope == program_scope);
  program_scope->insert_function_decl(*decl);
  return decl;
}

//TODO: add definition to current ast
StructDefinitionPtr
Sema::ActOnStructDefinition(Locatable loc, Token iden,
                            std::vector<DeclaratorDeclPtr> fields) {
  // 1. check if the struct already defined in this scope
  if (cur_scope->find_struct(iden.Text).has_value())
    errorloc(iden, "struct already defined");
  // 2. check field decl types and iden
  for (auto &field : fields) {
    const VariableDeclarator &decl =
        field->getDeclarator().as_variable_declarator();
    if (decl.getIdentifier() == iden.Text)
      errorloc(decl.getLoc(),
               "A structure or union shall not contain a member with  an "
               "instance of itself");
  }
  // 3. add struct definition into scope
  return std::make_unique<StructDefinition>(loc, iden.Text, std::move(fields));
}

StructSpecPtr Sema::ActOnStructSpec(Locatable loc, Symbol iden) {
  auto struct_def = cur_scope->find_struct(iden);
  if (struct_def.has_value())
    return std::make_unique<StructSpec>(loc, struct_def.value().get());
  else
    errorloc(loc, "Undefined struct ", iden, "'");
}

StructSpecPtr Sema::ActOnStructSpec(Locatable loc, StructDefinitionPtr struct_def) {
  return std::make_unique<StructSpec>(loc, std::move(struct_def));
}

DeclaratorDeclPtr Sema::ActOnDeclaration(Locatable loc, DeclSpecPtr spec,
                                         DeclaratorPtr decl) {
  switch (decl->getKind()) {
  case Declarator::Abstract:
    return std::make_unique<DeclaratorDecl>(
        loc, std::move(spec),
        std::unique_ptr<AbstractDeclarator>(
            static_cast<AbstractDeclarator *>(decl.release())));
  case Declarator::Named: {
    switch (decl->as_named_declarator().getKind()) {
    case NamedDeclarator::Function:
      return std::make_unique<DeclaratorDecl>(loc, std::move(spec),
                                              std::move(decl));
    case NamedDeclarator::Valued: {
      DeclaratorDeclPtr variable_decl = std::make_unique<DeclaratorDecl>(
          loc, std::move(spec), std::move(decl));

      cur_scope->insert_variable(*variable_decl);
      return variable_decl;
    }
    }
  }
  }
}

// Label names shall be unique within a function.
LabeledStmtPtr Sema::ActOnLabeledStmt(Locatable loc, LabelDeclPtr label_decl,
                                      StmtPtr stmt) {
  // push the LabelDecl * into function scope.
  // if within this function containes this label, error.
  if (cur_func_scope->find_label(label_decl->identifier()).has_value())
    errorloc(loc, "Redefinition of label '", label_decl->identifier(), "'");
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
      stmt.get().set_label_decl(&label_decl.value().get());
    else
      errorloc(stmt.get().getLoc(), "Use of undeclared label `", iden, '`');
  }
}

void Sema::enter_func_scope() {
  assert(cur_scope == program_scope);
  cur_func_scope = new FuncScope(program_scope);
  cur_scope = cur_func_scope;
}
void Sema::exit_func_scope() {
  assert(cur_scope == cur_func_scope);
  delete cur_func_scope;
  cur_scope = program_scope;
}

void Sema::enter_scope() {
  auto new_scope = new Scope(cur_scope);
  cur_scope = new_scope;
}

void Sema::exit_scope() {
  auto parent_scope = cur_scope;
  delete cur_scope;
  cur_scope = parent_scope;
}

// TODO:
void Sema::add_parameters(const FunctionDeclarator &function_declarator) {
  assert(cur_scope == cur_func_scope);
  auto params = function_declarator.getParams();
  std::for_each(params.begin(), params.end(),
                [&](std::reference_wrapper<const DeclaratorDecl> decl) {
                  cur_scope->insert_variable(decl.get());
                });
}
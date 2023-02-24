#include "Sema.hpp"
#include "Diagnostic.hpp"
#include "Scope.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxStmt.hpp"
#include <cstdlib>
#include <functional>
#include <memory>
#include <utility>

Sema::Sema() {
  program_scope = new ProgramScope();
  cur_scope = program_scope;
}
Sema::~Sema() { delete program_scope; }

FuncDefPtr Sema::ActOnFunctionDefinition(Locatable loc, DeclaratorDeclPtr decl,
                                         CompStmtPtr stmt) {
  auto function_definition =
      std::make_unique<FuncDef>(loc, std::move(decl), std::move(stmt));
  program_scope->insert_function_definition(*function_definition);
  return function_definition;
}
StructDefPtr Sema::ActOnStructDefinition(Locatable loc, Token iden,
                                         std::vector<DeclPtr> fields) {
  // 1. check if the struct already defined in this scope
  if (cur_scope->find_struct(iden.Text).has_value())
    errorloc(iden, "struct already defined");
  // 2. check field decl types and iden
  for (auto &field : fields) {
    if (field->getKind() != Decl::Declarator)
      errorloc(field->getLoc(), "Unexpected declarator type");
    const Declarator &declarator =
        static_cast<DeclaratorDecl &>(*field).getDeclarator();
    if (declarator.getKind() != Declarator::Valued)
      errorloc(declarator.getLoc(), "Unexpected declarator type");
    if (static_cast<const ValuedDeclarator &>(declarator).getIdentifier() ==
        iden.Text)
      errorloc(declarator.getLoc(),
               "A structure or union shall not contain a member with  an "
               "instance of itself");
  }
  // 3. add struct definition into scope
  return std::make_unique<StructDef>(loc, iden.Text, std::move(fields));
}

StructSpecPtr Sema::ActOnStructSpec(Locatable loc, Symbol iden) {
  auto struct_def = cur_scope->find_struct(iden);
  if (struct_def.has_value())
    return std::make_unique<StructSpec>(loc, struct_def.value().get());
  else
    errorloc(loc, "Undefined struct ", iden, "'");
}

StructSpecPtr Sema::ActOnStructSpec(Locatable loc, StructDefPtr struct_def) {
  return std::make_unique<StructSpec>(loc, std::move(struct_def));
}

DeclaratorDeclPtr Sema::ActOnDeclaration(Locatable loc, DeclSpecPtr spec,
                                         DeclaratorPtr decl) {
  switch (decl->getKind()) {
  case Declarator::Valued: {
    auto valued_declarator = std::unique_ptr<ValuedDeclarator>(
        static_cast<ValuedDeclarator *>(decl.release()));
    switch (valued_declarator->getKind()) {
    case ValuedDeclarator::Variable: {
      if (cur_scope->find_variable(valued_declarator->getIdentifier())
              .has_value())
        errorloc(decl->getLoc(), "variable already defined");
      auto variable = std::make_unique<DeclaratorDecl>(
          loc, std::move(spec), std::move(valued_declarator));
      cur_scope->insert_variable(*variable);
      return variable;
    }
    case ValuedDeclarator::FunctionVariable:
      auto function_variable = std::make_unique<DeclaratorDecl>(
          loc, std::move(spec), std::move(valued_declarator));
      cur_scope->insert_function_variable(*function_variable);
      return function_variable;
    }
  }
  case Declarator::Function: {
    if (cur_scope != program_scope)
      errorloc(loc, "function declared not in program scope");
    auto function =
        std::make_unique<DeclaratorDecl>(loc, std::move(spec), std::move(decl));
    program_scope->insert_function(*function);
    return function;
  }
  case Declarator::Abstract: {
    return std::make_unique<DeclaratorDecl>(
        loc, std::move(spec),
        std::unique_ptr<AbstractDeclarator>(
            static_cast<AbstractDeclarator *>(decl.release())));
  }
  }
}

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
      stmt.get().set_label_decl(&label_decl.value().get());
    else
      errorloc(stmt.get().getLoc(), "Use of undeclared label `", iden, '`');
  }
}

void Sema::enter_func_scope() {
  if (cur_scope != program_scope)
    exit(1); // TODO:
  cur_func_scope = new FuncScope(program_scope);
  cur_scope = cur_func_scope;
}
void Sema::exit_func_scope() {
  if (cur_scope != cur_func_scope)
    exit(1); // TODO:
  delete cur_func_scope;
  cur_scope = program_scope;
}
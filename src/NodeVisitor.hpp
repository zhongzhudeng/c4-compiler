#pragma once
#include "SyntaxDecl.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxStmt.hpp"

template <typename Derived> class ExprVisitor {
public:
#define DISPATCH(NAME, CLASS)                                                  \
  return static_cast<Derived &>(*this).Visit##NAME(                            \
      static_cast<const CLASS &>(expr))
  void visit(const Expr &expr) {
    switch (expr.getKind()) {
#define EXPR(DERIVED, BASE)                                                    \
  case Expr::DERIVED:                                                          \
    DISPATCH(DERIVED##Expr, DERIVED##Expr);
#define ABSTRACT_EXPR(EXPR)
#include "NodesExpr.def"
    }
  }
#undef DISPATCH
};

template <typename Derived> class DeclVisitor {
public:
  void visit(const Decl &decl) {
    switch (decl.getKind()) {
    case Decl::Declarator:
      return static_cast<Derived &>(*this).VisitDeclaratorDecl(
          static_cast<const DeclaratorDecl &>(decl));
    case Decl::LabelDecl:
      return static_cast<Derived &>(*this).VisitLabelDecl(
          static_cast<const LabelDecl &>(decl));
    case Decl::FuncDef:
      return static_cast<Derived &>(*this).VisitFuncDef(
          static_cast<const FuncDef &>(decl));
    case Decl::StructDef:
      return static_cast<Derived &>(*this).VisitStructDef(
          static_cast<const StructDef &>(decl));
    }
  }
  void VisitDeclSpec(const DeclSpec &declSpec) {
    switch (declSpec.getKind()) {
    case Type::Void:
      return static_cast<Derived &>(*this).VisitVoidSpec(
          static_cast<const VoidSpec &>(declSpec));
    case Type::Char:
      return static_cast<Derived &>(*this).VisitCharSpec(
          static_cast<const CharSpec &>(declSpec));
    case Type::Int:
      return static_cast<Derived &>(*this).VisitIntSpec(
          static_cast<const IntSpec &>(declSpec));
    case Type::Struct:
      return static_cast<Derived &>(*this).VisitStructSpec(
          static_cast<const StructSpec &>(declSpec));
    }
  }
  void VisitDeclarator(const Declarator &declarator) {
    switch (declarator.getKind()) {
    case Declarator::Named:
      return static_cast<Derived &>(*this).VisitNamedDeclarator(
          static_cast<const NamedDeclarator &>(declarator));

    case Declarator::Abstract:
      return static_cast<Derived &>(*this).VisitAbstractDeclarator(
          static_cast<const AbstractDeclarator &>(declarator));
    }
  }
  void VisitNamedDeclarator(const NamedDeclarator &declarator) {
    switch (declarator.getKind()) {
    case NamedDeclarator::Valued:
      return static_cast<Derived &>(*this).VisitValuedDeclarator(
          static_cast<const ValuedDeclarator &>(declarator));
    case NamedDeclarator::Function:
      return static_cast<Derived &>(*this).VisitFunctionDeclarator(
          static_cast<const FunctionDeclarator &>(declarator));
    }
  }
  void VisitValuedDeclarator(const ValuedDeclarator &declarator) {
    switch (declarator.getKind()) {
    case ValuedDeclarator::Variable:
      return static_cast<Derived &>(*this).VisitVariableDeclarator(
          static_cast<const VariableDeclarator &>(declarator));
    case ValuedDeclarator::FunctionVariable:
      return static_cast<Derived &>(*this).VisitFunctionVariableDeclarator(
          static_cast<const FunctionVariableDeclarator &>(declarator));
    }
  }
};

template <typename Derived> class StmtVisitor {
public:
#define DISPATCH(NAME, CLASS)                                                  \
  return static_cast<Derived &>(*this).Visit##NAME(                            \
      static_cast<const CLASS &>(stmt))
  void visit(const Stmt &stmt) {
    switch (stmt.getKind()) {
#define STMT(DERIVED, BASE)                                                    \
  case Stmt::DERIVED:                                                          \
    DISPATCH(DERIVED##Stmt, DERIVED##Stmt);
#define ABSTRACT_STMT(STMT)
#include "NodesStmt.def"
    }
  }
#undef DISPATCH
};
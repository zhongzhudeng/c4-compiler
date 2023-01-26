#include "Syntax.hpp"
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
  void VisitDeclSpec(const DeclSpec &declSpec) {
    switch (declSpec.getKind()) {
    case DeclSpec::Void:
      return static_cast<Derived &>(*this).VisitVoidSpec(
          static_cast<const VoidSpec &>(declSpec));
    case DeclSpec::Char:
      return static_cast<Derived &>(*this).VisitCharSpec(
          static_cast<const CharSpec &>(declSpec));
    case DeclSpec::Int:
      return static_cast<Derived &>(*this).VisitIntSpec(
          static_cast<const IntSpec &>(declSpec));
    case DeclSpec::Struct:
      return static_cast<Derived &>(*this).VisitStructSpec(
          static_cast<const StructSpec &>(declSpec));
    }
  }
  void VisitDeclarator(const Declarator &declarator) {
    switch (declarator.getKind()) {
    case Declarator::Primitive:
      return static_cast<Derived &>(*this).VisitPrimitiveDeclarator(
          static_cast<const PrimitiveDeclarator &>(declarator));
    case Declarator::Pointer:
      return static_cast<Derived &>(*this).VisitPointerDeclarator(
          static_cast<const PointerDeclarator &>(declarator));
    case Declarator::Function:
      return static_cast<Derived &>(*this).VisitFunctionDeclarator(
          static_cast<const FunctionDeclarator &>(declarator));
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
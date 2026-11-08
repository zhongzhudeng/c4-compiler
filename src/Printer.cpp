#include "NodeVisitor.hpp"
#include "SymbolInternalizer.hpp"
#include "Syntax.hpp"
#include "SyntaxDecl.hpp"
#include "SyntaxExpr.hpp"
#include "SyntaxStmt.hpp"
#include <cstddef>
#include <ostream>

class ExprPrinter : public ExprVisitor<ExprPrinter> {
  std::ostream &os;

public:
  ExprPrinter(std::ostream &os) : os(os) {}
  void VisitIdentifierExpr(const IdentifierExpr &expr) {
    os << *expr.getIdentifier();
  }
  void VisitConstantExpr(const ConstantExpr &expr) {
    os << *expr.getConstant();
  }
  void VisitStringLiteralExpr(const StringLiteralExpr &expr) {
    os << *expr.getStringLiteral();
  }
  void VisitParenExpr(const ParenExpr &expr) { visit(expr.getExpr()); }
  void VisitConditionalExpr(const ConditionalExpr &expr) {
    os << "(";
    visit(expr.getFirst());
    os << " ? ";
    visit(expr.getSecond());
    os << " : ";
    visit(expr.getThird());
    os << ")";
  }
  void VisitBinaryExpr(const BinaryExpr &expr) {
    os << "(";
    visit(expr.getLHS());
    os << ' ' << *expr.getOperator() << ' ';
    visit(expr.getRhS());
    os << ")";
  }
  void VisitUnaryExpr(const UnaryExpr &expr) {
    os << "(";
    os << *expr.getOperator();
    visit(expr.getExpr());
    os << ")";
  }
  void VisitSizeofExpr(const SizeofExpr &expr) {
    os << "(sizeof ";
    visit(expr.getExpr());
    os << ")";
  }
  void VisitSizeofTypeExpr(const SizeofTypeExpr &expr) {
    os << "(sizeof(" << *expr.getTypeName() << "))";
  }
  void VisitArraySubscriptExpr(const ArraySubscriptExpr &expr) {
    os << "(";
    visit(expr.getExpr());
    os << "[";
    visit(expr.getSquaredExpr());
    os << "])";
  }
  void VisitFunctionCallExpr(const FunctionCallExpr &expr) {
    os << "(";
    visit(expr.getCallee());
    os << "(";
    const std::vector<ExprPtr> &args = expr.getArgs();
    for (auto it = args.begin(); it != args.end() - 1; it++) {
      const Expr &arg = const_cast<Expr &>(**it);
      visit(arg);
      os << ", ";
    }
    const Expr &arg = const_cast<Expr &>(**(args.end() - 1));
    visit(arg);
    os << "))";
  }
  void VisitMemberExpr(const MemberExpr &expr) {
    os << "(";
    visit(expr.getExpr());
    os << *expr.getAccessOp();
    visit(expr.getMember());
    os << ")";
  }
};

class TranslationUnitPrinter
    : public TranslationUnitVisitor<TranslationUnitPrinter> {
  std::ostream &os;

public:
  TranslationUnitPrinter(std::ostream &os) : os(os) {}
  void visit(const TranslationUnit &ast) {
    auto declarations = ast.getDeclarations();
    for (auto it = declarations.begin(); it != declarations.end() - 1; it++) {
      it->get().print(os);
      if (not it->get().is_function_definition())
        os << ';';
      os << '\n';
    }
    declarations.back().get().print(os);
    if (not declarations.back().get().is_function_definition())
      os << ';';
  }
};

class DeclPrinter : public DeclVisitor<DeclPrinter> {
  std::ostream &os;
  unsigned indentation;

  std::ostream &indent() { return indent(indentation); }

public:
  DeclPrinter(std::ostream &os, unsigned indentation = 0)
      : os(os), indentation(indentation) {}
  std::ostream &indent(unsigned indentation) {
    for (unsigned i = 0; i != indentation; ++i)
      os << '\t';
    return os;
  }
  void VisitDeclaratorDecl(const DeclaratorDecl &decl) {
    VisitDeclSpec(decl.getDeclSpec());
    VisitDeclarator(decl.getDeclarator());
  }
  void VisitLabelDecl(const LabelDecl &decl) { os << *decl.iden(); }
  void VisitFuncDef(const FunctionDefinition &decl) {
    decl.getDecl().print(os);
    os << "\n";
    decl.getCompStmt().print(os);
    os << "\n";
  }
  void VisitStructDef(const StructDefinition &decl) {
    os << "struct " << *decl.iden() << "\n";
    indent();
    os << "{\n";
    indentation++;
    for (auto field_decl : decl.getFieldDecls()) {
      indent();
      visit(field_decl);
      os << ";";
    }
    os << '\n';
    indentation--;
    indent();
    os << "}";
  }
  void VisitVoidSpec([[maybe_unused]] const VoidSpec &spec) { os << "void"; }
  void VisitCharSpec([[maybe_unused]] const CharSpec &spec) { os << "char"; }
  void VisitIntSpec([[maybe_unused]] const IntSpec &spec) { os << "int"; }
  void VisitStructSpec(const StructSpec &spec) {
    if (spec.has_def())
      spec.getDef().print(os, indentation);
    else
      os << "struct " << *spec.getIdentifier();
  }
  void VisitVariableDeclarator(const ValuedDeclarator &decl) {
    os << ' ';
    PrintPointersFront(decl.pointer_size());
    os << *decl.getIdentifier();
    PrintPointersBack(decl.pointer_size());
  }
  void VisitFunctionVariableDeclarator(const FunctionVariableDeclarator &decl) {
    os << ' ';
    PrintPointersFront(decl.return_pointer_size());
    os << '(';
    PrintPointersFront(decl.pointer_size());
    os << *decl.getIdentifier();
    PrintPointersBack(decl.pointer_size());
    PrintParameters(decl.getParams());
    os << ')';
    PrintPointersBack(decl.return_pointer_size());
  }
  void VisitFunctionDeclarator(const FunctionDeclarator &decl) {
    os << ' ';
    for (size_t i = 0; i < decl.pointer_size(); i++)
      os << "(*";
    os << "(" << *decl.getIdentifier();
    PrintParameters(decl.getParams());
    os << ")";
    for (size_t i = 0; i < decl.pointer_size(); i++)
      os << ")";
  }
  void VisitAbstractDeclarator(const AbstractDeclarator &decl) {
    for (size_t i = 0; i < decl.pointer_size(); i++)
      os << "*";
  }
  void PrintParameters(
      const std::vector<std::reference_wrapper<const DeclaratorDecl>> &params) {
    os << "(";
    visit(params.front());
    for (auto it = params.begin() + 1; it != params.end(); it++) {
      os << ", ";
      visit(*it);
    }
    os << ')';
  }
  void PrintPointersFront(size_t n) {
    for (size_t i = 0; i < n; i++)
      os << "(*";
  }
  void PrintPointersBack(size_t n) {
    for (size_t i = 0; i < n; i++)
      os << ")";
  }
};

class StmtPrinter : public StmtVisitor<StmtPrinter> {
  std::ostream &os;
  unsigned indentation;
  std::ostream &indent() { return indent(indentation); }

public:
  StmtPrinter(std::ostream &os, unsigned indentation = 0)
      : os(os), indentation(indentation) {}
  std::ostream &indent(unsigned indentation) {
    for (unsigned i = 0; i != indentation; ++i)
      os << '\t';
    return os;
  }
  void VisitLabeledStmt(const LabeledStmt &stmt) {
    stmt.getLabelDecl().print(os);
    os << ":\n";
    visit(stmt.getStmt());
  }
  void VisitCompStmt(const CompStmt &stmt) {
    os << "{\n";
    indentation++;
    for (const Stmt &item : stmt.getBlockItemList()) {
      if (item.getKind() == Stmt::IfElse || item.getKind() == Stmt::Comp)
        indent();
      visit(item);
      os << '\n';
    }
    indentation--;
    indent();
    os << "}";
  }
  void VisitDeclStmt(const DeclStmt &stmt) {
    indent();
    stmt.getDeclaration().print(os, indentation);
    os << ";";
  }
  void VisitExprStmt(const ExprStmt &stmt) {
    indent();
    if (!stmt.isEmpty())
      stmt.getExpr().print(os);
    os << ";";
  }
  void VisitIfElseStmt(const IfElseStmt &stmt) {
    os << "if (";
    stmt.getCond().print(os);
    os << ")";
    const Stmt &then = stmt.getThen();
    if (then.getKind() == Stmt::Comp) {
      os << ' ';
      visit(then);
      if (stmt.isElseEmpty())
        return;
      os << " else";
    } else {
      os << '\n';
      indentation++;
      visit(then);
      indentation--;
      if (stmt.isElseEmpty())
        return;
      os << '\n';
      indent();
      os << "else";
    }

    const Stmt &Else = stmt.getElse();
    if (Else.getKind() == Stmt::IfElse || Else.getKind() == Stmt::Comp) {
      os << ' ';
      visit(Else);
    } else {
      os << '\n';
      indentation++;
      visit(Else);
      indentation--;
    }
  }
  void VisitWhileStmt(const WhileStmt &stmt) {
    indent();
    os << "while (";
    stmt.getCond().print(os);
    os << ")";
    const Stmt &doStmt = stmt.getDo();
    if (doStmt.getKind() == Stmt::Comp) {
      os << ' ';
      visit(doStmt);
    } else {
      os << '\n';
      indentation++;
      visit(doStmt);
      indentation--;
    }
  }
  void VisitGoToStmt(const GoToStmt &stmt) {
    indent();
    os << "goto ";
    stmt.getLabelDecl().print(os);
    os << ";";
  }
  void VisitContinueStmt([[maybe_unused]] const ContinueStmt &stmt) {
    indent();
    os << "continue;";
  }
  void VisitBreakStmt([[maybe_unused]] const BreakStmt &stmt) {
    indent();
    os << "break;";
  }
  void VisitReturnStmt(const ReturnStmt &stmt) {
    indent();
    os << "return";
    if (!stmt.isEmpty()) {
      os << ' ';
      stmt.getExpr().print(os);
    }
    os << ";";
  }
};

void Expr::print(std::ostream &os) const {
  ExprPrinter printer(os);
  printer.visit(const_cast<Expr &>(*this));
}

void TranslationUnit::print(std::ostream &os) const {
  TranslationUnitPrinter printer(os);
  printer.visit(const_cast<TranslationUnit &>(*this));
}

void Decl::print(std::ostream &os, unsigned identation) const {
  DeclPrinter printer(os, identation);
  printer.visit(const_cast<Decl &>(*this));
}

void Stmt::print(std::ostream &os, unsigned identation) const {
  StmtPrinter printer(os, identation);
  printer.visit(const_cast<Stmt &>(*this));
}
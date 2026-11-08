#include "Printer.hpp"

#include <cassert>
#include <variant>

Printer::Printer(const AST &ast, std::ostream &out) : ast_(ast), out_(out) {}

void Printer::indent(int level) {
  for (int i = 0; i < level; ++i)
    out_ << '\t';
}

const char *Printer::binaryOperator(BinaryExpr::Op op) {
  switch (op) {
  case BinaryExpr::Op::Mul:
    return "*";
  case BinaryExpr::Op::Add:
    return "+";
  case BinaryExpr::Op::Sub:
    return "-";
  case BinaryExpr::Op::Less:
    return "<";
  case BinaryExpr::Op::Equal:
    return "==";
  case BinaryExpr::Op::NotEqual:
    return "!=";
  case BinaryExpr::Op::LogicalAnd:
    return "&&";
  case BinaryExpr::Op::LogicalOr:
    return "||";
  }
  return "?";
}

const char *Printer::unaryOperator(UnaryExpr::Op op) {
  switch (op) {
  case UnaryExpr::Op::Plus:
    return "+";
  case UnaryExpr::Op::Minus:
    return "-";
  case UnaryExpr::Op::Not:
    return "!";
  case UnaryExpr::Op::Deref:
    return "*";
  case UnaryExpr::Op::Addr:
    return "&";
  }
  return "?";
}

bool Printer::isEmptyDeclarator(DeclaratorId id) const {
  if (!id.valid())
    return true;
  const Declarator &declarator = ast_.declarator(id);
  if (auto *name = std::get_if<NameDeclarator>(&declarator.node))
    return name->name.empty();
  return false;
}

bool Printer::isBarePointerDeclarator(DeclaratorId id) const {
  if (!id.valid())
    return false;
  auto *pointer = std::get_if<PointerDeclarator>(&ast_.declarator(id).node);
  return pointer && isEmptyDeclarator(pointer->inner);
}

void Printer::printDeclarator(DeclaratorId id) {
  if (!id.valid())
    return;

  const Declarator &declarator = ast_.declarator(id);
  if (auto *name = std::get_if<NameDeclarator>(&declarator.node)) {
    if (name->name)
      out_ << *name->name;
    return;
  }

  if (auto *pointer = std::get_if<PointerDeclarator>(&declarator.node)) {
    if (isEmptyDeclarator(pointer->inner)) {
      out_ << '*';
      return;
    }
    out_ << "(*";
    printDeclarator(pointer->inner);
    out_ << ')';
    return;
  }

  auto *function = std::get_if<FunctionDeclarator>(&declarator.node);
  assert(function && "unknown declarator kind");

  if (isEmptyDeclarator(function->inner)) {
    out_ << '(';
    printParameterList(*function);
    out_ << ')';
    return;
  }

  out_ << '(';
  if (isBarePointerDeclarator(function->inner))
    out_ << "(*)";
  else
    printDeclarator(function->inner);
  out_ << '(';
  printParameterList(*function);
  out_ << "))";
}

void Printer::printParameterList(const FunctionDeclarator &function) {
  if (function.explicitVoid) {
    out_ << "void";
    return;
  }
  for (size_t i = 0; i < function.params.size(); ++i) {
    if (i)
      out_ << ", ";
    printParameter(function.params[i]);
  }
}

void Printer::printParameter(DeclId id) {
  auto *parameter = std::get_if<ParamDecl>(&ast_.decl(id).node);
  if (!parameter)
    return;
  printFlatTypeAndDeclarator(parameter->spec, parameter->declarator);
}

void Printer::printFlatTypeSpec(TypeSpecId id) {
  const TypeSpec &spec = ast_.typeSpec(id);
  if (std::holds_alternative<TypeSpec::Void>(spec.node)) {
    out_ << "void";
    return;
  }
  if (std::holds_alternative<TypeSpec::Char>(spec.node)) {
    out_ << "char";
    return;
  }
  if (std::holds_alternative<TypeSpec::Int>(spec.node)) {
    out_ << "int";
    return;
  }

  const TypeSpec::Struct &structure = std::get<TypeSpec::Struct>(spec.node);
  out_ << "struct";
  if (structure.tag)
    out_ << ' ' << *structure.tag;
  if (structure.hasBraces) {
    out_ << " {";
    for (DeclId field : structure.fields) {
      out_ << ' ';
      printFlatField(field);
    }
    out_ << " }";
  }
}

void Printer::printFlatField(DeclId id) {
  auto *field = std::get_if<FieldDecl>(&ast_.decl(id).node);
  if (!field)
    return;
  printFlatTypeAndDeclarator(field->spec, field->declarator);
  out_ << ';';
}

void Printer::printFlatTypeAndDeclarator(TypeSpecId spec,
                                         DeclaratorId declarator) {
  printFlatTypeSpec(spec);
  if (!isEmptyDeclarator(declarator)) {
    out_ << ' ';
    printDeclarator(declarator);
  }
}

void Printer::printTypeAndDeclarator(TypeSpecId spec, DeclaratorId declarator,
                                     int level) {
  printTypeSpec(spec, level);
  if (!isEmptyDeclarator(declarator)) {
    out_ << ' ';
    printDeclarator(declarator);
  }
}

void Printer::printExpr(ExprId id) {
  if (!id.valid())
    return;

  const Expr &expr = ast_.expr(id);
  if (auto *identifier = std::get_if<IdentifierExpr>(&expr.node)) {
    if (identifier->name)
      out_ << *identifier->name;
    return;
  }
  if (auto *constant = std::get_if<ConstantExpr>(&expr.node)) {
    if (constant->spelling)
      out_ << *constant->spelling;
    return;
  }
  if (auto *string = std::get_if<StringLiteralExpr>(&expr.node)) {
    if (string->spelling)
      out_ << *string->spelling;
    return;
  }

  if (auto *unary = std::get_if<UnaryExpr>(&expr.node)) {
    out_ << '(' << unaryOperator(unary->op);
    printExpr(unary->operand);
    out_ << ')';
    return;
  }

  if (auto *binary = std::get_if<BinaryExpr>(&expr.node)) {
    out_ << '(';
    printExpr(binary->lhs);
    out_ << ' ' << binaryOperator(binary->op) << ' ';
    printExpr(binary->rhs);
    out_ << ')';
    return;
  }

  if (auto *assign = std::get_if<AssignExpr>(&expr.node)) {
    out_ << '(';
    printExpr(assign->lhs);
    out_ << " = ";
    printExpr(assign->rhs);
    out_ << ')';
    return;
  }

  if (auto *conditional = std::get_if<ConditionalExpr>(&expr.node)) {
    out_ << '(';
    printExpr(conditional->cond);
    out_ << " ? ";
    printExpr(conditional->thenExpr);
    out_ << " : ";
    printExpr(conditional->elseExpr);
    out_ << ')';
    return;
  }

  if (auto *call = std::get_if<CallExpr>(&expr.node)) {
    out_ << '(';
    printExpr(call->callee);
    out_ << '(';
    for (size_t i = 0; i < call->args.size(); ++i) {
      if (i)
        out_ << ", ";
      printExpr(call->args[i]);
    }
    out_ << "))";
    return;
  }

  if (auto *subscript = std::get_if<SubscriptExpr>(&expr.node)) {
    out_ << '(';
    printExpr(subscript->base);
    out_ << '[';
    printExpr(subscript->index);
    out_ << "])";
    return;
  }

  if (auto *member = std::get_if<MemberExpr>(&expr.node)) {
    out_ << '(';
    printExpr(member->base);
    out_ << (member->arrow ? "->" : ".");
    if (member->member)
      out_ << *member->member;
    out_ << ')';
    return;
  }

  auto *sizeOf = std::get_if<SizeofExpr>(&expr.node);
  assert(sizeOf && "unknown expression kind");
  if (auto *typeName = std::get_if<SizeofExpr::TypeName>(&sizeOf->target)) {
    out_ << "(sizeof(";
    printFlatTypeSpec(typeName->spec);
    if (!isEmptyDeclarator(typeName->declarator)) {
      out_ << ' ';
      printDeclarator(typeName->declarator);
    }
    out_ << "))";
    return;
  }
  out_ << "(sizeof ";
  printExpr(std::get<ExprId>(sizeOf->target));
  out_ << ')';
}

void Printer::printTypeSpec(TypeSpecId id, int level) {
  const TypeSpec &spec = ast_.typeSpec(id);
  if (std::holds_alternative<TypeSpec::Void>(spec.node)) {
    out_ << "void";
    return;
  }
  if (std::holds_alternative<TypeSpec::Char>(spec.node)) {
    out_ << "char";
    return;
  }
  if (std::holds_alternative<TypeSpec::Int>(spec.node)) {
    out_ << "int";
    return;
  }

  const TypeSpec::Struct &structure = std::get<TypeSpec::Struct>(spec.node);
  out_ << "struct";
  if (structure.tag)
    out_ << ' ' << *structure.tag;
  if (structure.hasBraces) {
    out_ << '\n';
    indent(level);
    out_ << "{\n";
    for (DeclId field : structure.fields)
      printDecl(field, level + 1);
    indent(level);
    out_ << "}";
  }
}

void Printer::printDecl(DeclId id, int level) {
  const Decl &decl = ast_.decl(id);

  if (auto *variable = std::get_if<VarDecl>(&decl.node)) {
    indent(level);
    printTypeAndDeclarator(variable->spec, variable->declarator, level);
    out_ << ";\n";
    return;
  }

  if (auto *function = std::get_if<FunctionDecl>(&decl.node)) {
    indent(level);
    printTypeSpec(function->spec, level);
    out_ << ' ';
    printDeclarator(function->declarator);
    if (!function->body.valid()) {
      out_ << ";\n";
      return;
    }
    out_ << '\n';
    printCompoundBody(function->body, level);
    return;
  }

  if (auto *field = std::get_if<FieldDecl>(&decl.node)) {
    indent(level);
    printTypeAndDeclarator(field->spec, field->declarator, level);
    out_ << ";\n";
    return;
  }

  if (auto *parameter = std::get_if<ParamDecl>(&decl.node)) {
    printTypeAndDeclarator(parameter->spec, parameter->declarator, level);
    return;
  }

  auto *tag = std::get_if<TagDecl>(&decl.node);
  assert(tag && "unknown declaration kind");
  indent(level);
  printTypeSpec(tag->spec, level);
  out_ << ";\n";
}

void Printer::printCompound(StmtId id, int level, bool leadingIndent,
                            bool trailingNewline) {
  auto *compound = std::get_if<CompoundStmt>(&ast_.stmt(id).node);
  assert(compound && "expected a compound statement");
  if (leadingIndent)
    indent(level);
  out_ << "{\n";
  for (StmtId item : compound->items)
    printStmt(item, level + 1);
  indent(level);
  out_ << '}';
  if (trailingNewline)
    out_ << '\n';
}

void Printer::printCompoundBody(StmtId id, int level) {
  printCompound(id, level, /*leadingIndent=*/true, /*trailingNewline=*/true);
}

void Printer::printCompoundInline(StmtId id, int level) {
  printCompound(id, level, /*leadingIndent=*/false, /*trailingNewline=*/false);
}

void Printer::printControlledBody(StmtId id, int level) {
  if (std::holds_alternative<CompoundStmt>(ast_.stmt(id).node)) {
    out_ << ' ';
    printCompoundInline(id, level);
    return;
  }
  out_ << '\n';
  printStmt(id, level + 1);
}

void Printer::printIf(StmtId id, int level, bool leadingIndent) {
  auto *ifStmt = std::get_if<IfStmt>(&ast_.stmt(id).node);
  assert(ifStmt && "expected an if statement");

  if (leadingIndent)
    indent(level);
  out_ << "if (";
  printExpr(ifStmt->cond);
  out_ << ')';

  bool thenIsCompound =
      std::holds_alternative<CompoundStmt>(ast_.stmt(ifStmt->thenStmt).node);
  printControlledBody(ifStmt->thenStmt, level);

  if (ifStmt->elseStmt.valid()) {
    if (thenIsCompound) {
      out_ << " else";
    } else {
      indent(level);
      out_ << "else";
    }

    const Stmt &elseStmt = ast_.stmt(ifStmt->elseStmt);
    if (std::holds_alternative<IfStmt>(elseStmt.node)) {
      out_ << ' ';
      printIf(ifStmt->elseStmt, level, false);
    } else if (std::holds_alternative<CompoundStmt>(elseStmt.node)) {
      out_ << ' ';
      printCompoundInline(ifStmt->elseStmt, level);
      out_ << '\n';
    } else {
      out_ << '\n';
      printStmt(ifStmt->elseStmt, level + 1);
    }
  } else if (thenIsCompound) {
    out_ << '\n';
  }
}

void Printer::printStmt(StmtId id, int level) {
  const Stmt &stmt = ast_.stmt(id);

  if (std::holds_alternative<CompoundStmt>(stmt.node)) {
    printCompoundBody(id, level);
    return;
  }

  if (auto *declStmt = std::get_if<DeclStmt>(&stmt.node)) {
    printDecl(declStmt->decl, level);
    return;
  }

  if (auto *exprStmt = std::get_if<ExprStmt>(&stmt.node)) {
    indent(level);
    printExpr(exprStmt->expr);
    out_ << ";\n";
    return;
  }

  if (std::holds_alternative<IfStmt>(stmt.node)) {
    printIf(id, level, true);
    return;
  }

  if (auto *whileStmt = std::get_if<WhileStmt>(&stmt.node)) {
    indent(level);
    out_ << "while (";
    printExpr(whileStmt->cond);
    out_ << ')';
    bool bodyIsCompound =
        std::holds_alternative<CompoundStmt>(ast_.stmt(whileStmt->body).node);
    printControlledBody(whileStmt->body, level);
    if (bodyIsCompound)
      out_ << '\n';
    return;
  }

  if (auto *returnStmt = std::get_if<ReturnStmt>(&stmt.node)) {
    indent(level);
    out_ << "return";
    if (returnStmt->expr.valid()) {
      out_ << ' ';
      printExpr(returnStmt->expr);
    }
    out_ << ";\n";
    return;
  }

  if (std::holds_alternative<BreakStmt>(stmt.node)) {
    indent(level);
    out_ << "break;\n";
    return;
  }

  if (std::holds_alternative<ContinueStmt>(stmt.node)) {
    indent(level);
    out_ << "continue;\n";
    return;
  }

  if (auto *gotoStmt = std::get_if<GotoStmt>(&stmt.node)) {
    indent(level);
    out_ << "goto ";
    if (gotoStmt->name)
      out_ << *gotoStmt->name;
    out_ << ";\n";
    return;
  }

  auto *labelStmt = std::get_if<LabelStmt>(&stmt.node);
  assert(labelStmt && "unknown statement kind");
  const LabelDecl &label = ast_.label(labelStmt->label);
  if (label.name)
    out_ << *label.name;
  out_ << ":\n";
  printStmt(labelStmt->stmt, level);
}

void Printer::print() {
  const auto &declarations = ast_.translationUnit().decls;
  for (size_t i = 0; i < declarations.size(); ++i) {
    printDecl(declarations[i], 0);
    if (i + 1 < declarations.size())
      out_ << '\n';
  }
}

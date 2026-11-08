#include "Printer.hpp"

#include <cassert>
#include <variant>

Printer::Printer(const AST &ast, const SemanticModel &model,
                 std::ostream &out)
    : ast_(ast), out_(out) {
  (void)model;
}

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

std::string Printer::declaratorString(DeclaratorId id) const {
  if (!id.valid())
    return {};

  const Declarator &declarator = ast_.declarator(id);
  if (auto *name = std::get_if<NameDeclarator>(&declarator.node))
    return name->name ? *name->name : std::string();

  if (auto *pointer = std::get_if<PointerDeclarator>(&declarator.node)) {
    std::string inner = declaratorString(pointer->inner);
    if (inner.empty())
      return "*";
    return "(*" + inner + ")";
  }

  auto *function = std::get_if<FunctionDeclarator>(&declarator.node);
  assert(function && "unknown declarator kind");
  std::string inner = declaratorString(function->inner);

  std::string parameters;
  if (function->explicitVoid) {
    parameters = "void";
  } else {
    for (size_t i = 0; i < function->params.size(); ++i) {
      if (i)
        parameters += ", ";
      parameters += parameterString(function->params[i]);
    }
  }

  // An abstract pointer is a single-token atom when it stands alone, but it
  // needs parentheses when a function suffix is attached to it.
  if (inner == "*")
    inner = "(*)";

  if (inner.empty())
    return "(" + parameters + ")";
  return "(" + inner + "(" + parameters + "))";
}

std::string Printer::parameterString(DeclId id) const {
  auto *parameter = std::get_if<ParamDecl>(&ast_.decl(id).node);
  if (!parameter)
    return {};
  std::string result = typeSpecFlat(parameter->spec);
  std::string declarator = declaratorString(parameter->declarator);
  if (!declarator.empty())
    result += " " + declarator;
  return result;
}

std::string Printer::typeSpecFlat(TypeSpecId id) const {
  const TypeSpec &spec = ast_.typeSpec(id);
  if (std::holds_alternative<TypeSpec::Void>(spec.node))
    return "void";
  if (std::holds_alternative<TypeSpec::Char>(spec.node))
    return "char";
  if (std::holds_alternative<TypeSpec::Int>(spec.node))
    return "int";

  const TypeSpec::Struct &structure = std::get<TypeSpec::Struct>(spec.node);
  std::string result = "struct";
  if (structure.tag)
    result += " " + *structure.tag;
  if (structure.hasBraces) {
    result += " {";
    for (DeclId field : structure.fields)
      result += " " + fieldFlat(field);
    result += " }";
  }
  return result;
}

std::string Printer::fieldFlat(DeclId id) const {
  auto *field = std::get_if<FieldDecl>(&ast_.decl(id).node);
  if (!field)
    return {};
  std::string result = typeSpecFlat(field->spec);
  std::string declarator = declaratorString(field->declarator);
  if (!declarator.empty())
    result += " " + declarator;
  result += ";";
  return result;
}

std::string Printer::expressionString(ExprId id) const {
  if (!id.valid())
    return {};

  const Expr &expr = ast_.expr(id);
  if (auto *identifier = std::get_if<IdentifierExpr>(&expr.node))
    return identifier->name ? *identifier->name : std::string();
  if (auto *constant = std::get_if<ConstantExpr>(&expr.node))
    return constant->spelling ? *constant->spelling : std::string();
  if (auto *string = std::get_if<StringLiteralExpr>(&expr.node))
    return string->spelling ? *string->spelling : std::string();

  if (auto *unary = std::get_if<UnaryExpr>(&expr.node)) {
    const char *op = "";
    switch (unary->op) {
    case UnaryExpr::Op::Plus:
      op = "+";
      break;
    case UnaryExpr::Op::Minus:
      op = "-";
      break;
    case UnaryExpr::Op::Not:
      op = "!";
      break;
    case UnaryExpr::Op::Deref:
      op = "*";
      break;
    case UnaryExpr::Op::Addr:
      op = "&";
      break;
    }
    return "(" + std::string(op) + expressionString(unary->operand) + ")";
  }

  if (auto *binary = std::get_if<BinaryExpr>(&expr.node))
    return "(" + expressionString(binary->lhs) + " " +
           binaryOperator(binary->op) + " " +
           expressionString(binary->rhs) + ")";

  if (auto *assign = std::get_if<AssignExpr>(&expr.node))
    return "(" + expressionString(assign->lhs) + " = " +
           expressionString(assign->rhs) + ")";

  if (auto *conditional = std::get_if<ConditionalExpr>(&expr.node))
    return "(" + expressionString(conditional->cond) + " ? " +
           expressionString(conditional->thenExpr) + " : " +
           expressionString(conditional->elseExpr) + ")";

  if (auto *call = std::get_if<CallExpr>(&expr.node)) {
    std::string result = "(" + expressionString(call->callee) + "(";
    for (size_t i = 0; i < call->args.size(); ++i) {
      if (i)
        result += ", ";
      result += expressionString(call->args[i]);
    }
    result += "))";
    return result;
  }

  if (auto *subscript = std::get_if<SubscriptExpr>(&expr.node))
    return "(" + expressionString(subscript->base) + "[" +
           expressionString(subscript->index) + "])";

  if (auto *member = std::get_if<MemberExpr>(&expr.node))
    return "(" + expressionString(member->base) +
           (member->arrow ? "->" : ".") +
           (member->member ? *member->member : std::string()) + ")";

  auto *sizeOf = std::get_if<SizeofExpr>(&expr.node);
  assert(sizeOf && "unknown expression kind");
  if (auto *typeName = std::get_if<SizeofExpr::TypeName>(&sizeOf->target)) {
    std::string spelling = typeSpecFlat(typeName->spec);
    std::string declarator = declaratorString(typeName->declarator);
    if (!declarator.empty())
      spelling += " " + declarator;
    return "(sizeof(" + spelling + "))";
  }
  return "(sizeof " + expressionString(std::get<ExprId>(sizeOf->target)) +
         ")";
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
    printTypeSpec(variable->spec, level);
    std::string declarator = declaratorString(variable->declarator);
    if (!declarator.empty())
      out_ << ' ' << declarator;
    out_ << ";\n";
    return;
  }

  if (auto *function = std::get_if<FunctionDecl>(&decl.node)) {
    indent(level);
    printTypeSpec(function->spec, level);
    out_ << ' ' << declaratorString(function->declarator);
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
    printTypeSpec(field->spec, level);
    std::string declarator = declaratorString(field->declarator);
    if (!declarator.empty())
      out_ << ' ' << declarator;
    out_ << ";\n";
    return;
  }

  if (auto *parameter = std::get_if<ParamDecl>(&decl.node)) {
    printTypeSpec(parameter->spec, level);
    std::string declarator = declaratorString(parameter->declarator);
    if (!declarator.empty())
      out_ << ' ' << declarator;
    return;
  }

  auto *tag = std::get_if<TagDecl>(&decl.node);
  assert(tag && "unknown declaration kind");
  indent(level);
  printTypeSpec(tag->spec, level);
  out_ << ";\n";
}

void Printer::printCompoundBody(StmtId id, int level) {
  auto *compound = std::get_if<CompoundStmt>(&ast_.stmt(id).node);
  assert(compound && "expected a compound statement");
  indent(level);
  out_ << "{\n";
  for (StmtId item : compound->items)
    printStmt(item, level + 1);
  indent(level);
  out_ << "}\n";
}

void Printer::printCompoundInline(StmtId id, int level) {
  auto *compound = std::get_if<CompoundStmt>(&ast_.stmt(id).node);
  assert(compound && "expected a compound statement");
  out_ << "{\n";
  for (StmtId item : compound->items)
    printStmt(item, level + 1);
  indent(level);
  out_ << "}";
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
  out_ << "if (" << expressionString(ifStmt->cond) << ")";

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
    if (exprStmt->expr.valid())
      out_ << expressionString(exprStmt->expr);
    out_ << ";\n";
    return;
  }

  if (std::holds_alternative<IfStmt>(stmt.node)) {
    printIf(id, level, true);
    return;
  }

  if (auto *whileStmt = std::get_if<WhileStmt>(&stmt.node)) {
    indent(level);
    out_ << "while (" << expressionString(whileStmt->cond) << ")";
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
    if (returnStmt->expr.valid())
      out_ << ' ' << expressionString(returnStmt->expr);
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
    out_ << "goto " << (gotoStmt->name ? *gotoStmt->name : std::string())
         << ";\n";
    return;
  }

  auto *labelStmt = std::get_if<LabelStmt>(&stmt.node);
  assert(labelStmt && "unknown statement kind");
  const LabelDecl &label = ast_.label(labelStmt->label);
  out_ << (label.name ? *label.name : std::string()) << ":\n";
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

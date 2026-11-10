#include "Printer.hpp"

#include <cassert>
#include <variant>

Printer::Printer(const AST& ast, std::ostream& out) : ast_(ast), out_(out) {}

void Printer::indent(int level) {
    for (int i = 0; i < level; ++i)
        out_ << '\t';
}

const char* Printer::binary_operator(BinaryExpr::Op op) {
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

const char* Printer::unary_operator(UnaryExpr::Op op) {
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

bool Printer::is_empty_declarator(DeclaratorId id) const {
    if (!id.valid())
        return true;
    const Declarator& declarator = ast_.declarator(id);
    if (auto* name = std::get_if<NameDeclarator>(&declarator.node))
        return name->name.empty();
    return false;
}

bool Printer::is_bare_pointer_declarator(DeclaratorId id) const {
    if (!id.valid())
        return false;
    auto* pointer = std::get_if<PointerDeclarator>(&ast_.declarator(id).node);
    return pointer && is_empty_declarator(pointer->inner);
}

void Printer::print_declarator(DeclaratorId id) {
    if (!id.valid())
        return;

    const Declarator& declarator = ast_.declarator(id);
    if (auto* name = std::get_if<NameDeclarator>(&declarator.node)) {
        if (name->name)
            out_ << *name->name;
        return;
    }

    if (auto* pointer = std::get_if<PointerDeclarator>(&declarator.node)) {
        if (is_empty_declarator(pointer->inner)) {
            out_ << '*';
            return;
        }
        out_ << "(*";
        print_declarator(pointer->inner);
        out_ << ')';
        return;
    }

    auto* function = std::get_if<FunctionDeclarator>(&declarator.node);
    assert(function && "unknown declarator kind");

    if (is_empty_declarator(function->inner)) {
        out_ << '(';
        print_parameter_list(*function);
        out_ << ')';
        return;
    }

    out_ << '(';
    if (is_bare_pointer_declarator(function->inner))
        out_ << "(*)";
    else
        print_declarator(function->inner);
    out_ << '(';
    print_parameter_list(*function);
    out_ << "))";
}

void Printer::print_parameter_list(const FunctionDeclarator& function) {
    if (function.explicit_void) {
        out_ << "void";
        return;
    }
    for (size_t i = 0; i < function.params.size(); ++i) {
        if (i)
            out_ << ", ";
        print_parameter(function.params[i]);
    }
}

void Printer::print_parameter(DeclId id) {
    auto* parameter = std::get_if<ParamDecl>(&ast_.decl(id).node);
    if (!parameter)
        return;
    print_flat_type_and_declarator(parameter->spec, parameter->declarator);
}

void Printer::print_flat_type_spec(TypeSpecId id) {
    const TypeSpec& spec = ast_.type_spec(id);
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

    const TypeSpec::Struct& structure = std::get<TypeSpec::Struct>(spec.node);
    out_ << "struct";
    if (structure.tag)
        out_ << ' ' << *structure.tag;
    if (structure.has_braces) {
        out_ << " {";
        for (DeclId field : structure.fields) {
            out_ << ' ';
            print_flat_field(field);
        }
        out_ << " }";
    }
}

void Printer::print_flat_field(DeclId id) {
    auto* field = std::get_if<FieldDecl>(&ast_.decl(id).node);
    if (!field)
        return;
    print_flat_type_and_declarator(field->spec, field->declarator);
    out_ << ';';
}

void Printer::print_flat_type_and_declarator(TypeSpecId spec, DeclaratorId declarator) {
    print_flat_type_spec(spec);
    if (!is_empty_declarator(declarator)) {
        out_ << ' ';
        print_declarator(declarator);
    }
}

void Printer::print_type_and_declarator(TypeSpecId spec, DeclaratorId declarator, int level) {
    print_type_spec(spec, level);
    if (!is_empty_declarator(declarator)) {
        out_ << ' ';
        print_declarator(declarator);
    }
}

void Printer::print_expr(ExprId id) {
    if (!id.valid())
        return;

    const Expr& expr = ast_.expr(id);
    if (auto* identifier = std::get_if<IdentifierExpr>(&expr.node)) {
        if (identifier->name)
            out_ << *identifier->name;
        return;
    }
    if (auto* constant = std::get_if<ConstantExpr>(&expr.node)) {
        if (constant->spelling)
            out_ << *constant->spelling;
        return;
    }
    if (auto* string = std::get_if<StringLiteralExpr>(&expr.node)) {
        if (string->spelling)
            out_ << *string->spelling;
        return;
    }

    if (auto* unary = std::get_if<UnaryExpr>(&expr.node)) {
        out_ << '(' << unary_operator(unary->op);
        print_expr(unary->operand);
        out_ << ')';
        return;
    }

    if (auto* binary = std::get_if<BinaryExpr>(&expr.node)) {
        out_ << '(';
        print_expr(binary->lhs);
        out_ << ' ' << binary_operator(binary->op) << ' ';
        print_expr(binary->rhs);
        out_ << ')';
        return;
    }

    if (auto* assign = std::get_if<AssignExpr>(&expr.node)) {
        out_ << '(';
        print_expr(assign->lhs);
        out_ << " = ";
        print_expr(assign->rhs);
        out_ << ')';
        return;
    }

    if (auto* conditional = std::get_if<ConditionalExpr>(&expr.node)) {
        out_ << '(';
        print_expr(conditional->cond);
        out_ << " ? ";
        print_expr(conditional->then_expr);
        out_ << " : ";
        print_expr(conditional->else_expr);
        out_ << ')';
        return;
    }

    if (auto* call = std::get_if<CallExpr>(&expr.node)) {
        out_ << '(';
        print_expr(call->callee);
        out_ << '(';
        for (size_t i = 0; i < call->args.size(); ++i) {
            if (i)
                out_ << ", ";
            print_expr(call->args[i]);
        }
        out_ << "))";
        return;
    }

    if (auto* subscript = std::get_if<SubscriptExpr>(&expr.node)) {
        out_ << '(';
        print_expr(subscript->base);
        out_ << '[';
        print_expr(subscript->index);
        out_ << "])";
        return;
    }

    if (auto* member = std::get_if<MemberExpr>(&expr.node)) {
        out_ << '(';
        print_expr(member->base);
        out_ << (member->arrow ? "->" : ".");
        if (member->member)
            out_ << *member->member;
        out_ << ')';
        return;
    }

    auto* size_of = std::get_if<SizeofExpr>(&expr.node);
    assert(size_of && "unknown expression kind");
    if (auto* type_name = std::get_if<SizeofExpr::TypeName>(&size_of->target)) {
        out_ << "(sizeof(";
        print_flat_type_spec(type_name->spec);
        if (!is_empty_declarator(type_name->declarator)) {
            out_ << ' ';
            print_declarator(type_name->declarator);
        }
        out_ << "))";
        return;
    }
    out_ << "(sizeof ";
    print_expr(std::get<ExprId>(size_of->target));
    out_ << ')';
}

void Printer::print_type_spec(TypeSpecId id, int level) {
    const TypeSpec& spec = ast_.type_spec(id);
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

    const TypeSpec::Struct& structure = std::get<TypeSpec::Struct>(spec.node);
    out_ << "struct";
    if (structure.tag)
        out_ << ' ' << *structure.tag;
    if (structure.has_braces) {
        out_ << '\n';
        indent(level);
        out_ << "{\n";
        for (DeclId field : structure.fields)
            print_decl(field, level + 1);
        indent(level);
        out_ << "}";
    }
}

void Printer::print_decl(DeclId id, int level) {
    const Decl& decl = ast_.decl(id);

    if (auto* variable = std::get_if<VarDecl>(&decl.node)) {
        indent(level);
        print_type_and_declarator(variable->spec, variable->declarator, level);
        out_ << ";\n";
        return;
    }

    if (auto* function = std::get_if<FunctionDecl>(&decl.node)) {
        indent(level);
        print_type_spec(function->spec, level);
        out_ << ' ';
        print_declarator(function->declarator);
        if (!function->body.valid()) {
            out_ << ";\n";
            return;
        }
        out_ << '\n';
        print_compound_body(function->body, level);
        return;
    }

    if (auto* field = std::get_if<FieldDecl>(&decl.node)) {
        indent(level);
        print_type_and_declarator(field->spec, field->declarator, level);
        out_ << ";\n";
        return;
    }

    if (auto* parameter = std::get_if<ParamDecl>(&decl.node)) {
        print_type_and_declarator(parameter->spec, parameter->declarator, level);
        return;
    }

    auto* tag = std::get_if<TagDecl>(&decl.node);
    assert(tag && "unknown declaration kind");
    indent(level);
    print_type_spec(tag->spec, level);
    out_ << ";\n";
}

void Printer::print_compound(StmtId id, int level, bool leading_indent, bool trailing_newline) {
    auto* compound = std::get_if<CompoundStmt>(&ast_.stmt(id).node);
    assert(compound && "expected a compound statement");
    if (leading_indent)
        indent(level);
    out_ << "{\n";
    for (StmtId item : compound->items)
        print_stmt(item, level + 1);
    indent(level);
    out_ << '}';
    if (trailing_newline)
        out_ << '\n';
}

void Printer::print_compound_body(StmtId id, int level) {
    print_compound(id, level, /*leadingIndent=*/true, /*trailingNewline=*/true);
}

void Printer::print_compound_inline(StmtId id, int level) {
    print_compound(id, level, /*leadingIndent=*/false, /*trailingNewline=*/false);
}

void Printer::print_controlled_body(StmtId id, int level) {
    if (std::holds_alternative<CompoundStmt>(ast_.stmt(id).node)) {
        out_ << ' ';
        print_compound_inline(id, level);
        return;
    }
    out_ << '\n';
    print_stmt(id, level + 1);
}

void Printer::print_if(StmtId id, int level, bool leading_indent) {
    auto* if_stmt = std::get_if<IfStmt>(&ast_.stmt(id).node);
    assert(if_stmt && "expected an if statement");

    if (leading_indent)
        indent(level);
    out_ << "if (";
    print_expr(if_stmt->cond);
    out_ << ')';

    bool then_is_compound =
        std::holds_alternative<CompoundStmt>(ast_.stmt(if_stmt->then_stmt).node);
    print_controlled_body(if_stmt->then_stmt, level);

    if (if_stmt->else_stmt.valid()) {
        if (then_is_compound) {
            out_ << " else";
        } else {
            indent(level);
            out_ << "else";
        }

        const Stmt& else_stmt = ast_.stmt(if_stmt->else_stmt);
        if (std::holds_alternative<IfStmt>(else_stmt.node)) {
            out_ << ' ';
            print_if(if_stmt->else_stmt, level, false);
        } else if (std::holds_alternative<CompoundStmt>(else_stmt.node)) {
            out_ << ' ';
            print_compound_inline(if_stmt->else_stmt, level);
            out_ << '\n';
        } else {
            out_ << '\n';
            print_stmt(if_stmt->else_stmt, level + 1);
        }
    } else if (then_is_compound) {
        out_ << '\n';
    }
}

void Printer::print_stmt(StmtId id, int level) {
    const Stmt& stmt = ast_.stmt(id);

    if (std::holds_alternative<CompoundStmt>(stmt.node)) {
        print_compound_body(id, level);
        return;
    }

    if (auto* decl_stmt = std::get_if<DeclStmt>(&stmt.node)) {
        print_decl(decl_stmt->decl, level);
        return;
    }

    if (auto* expr_stmt = std::get_if<ExprStmt>(&stmt.node)) {
        indent(level);
        print_expr(expr_stmt->expr);
        out_ << ";\n";
        return;
    }

    if (std::holds_alternative<IfStmt>(stmt.node)) {
        print_if(id, level, true);
        return;
    }

    if (auto* while_stmt = std::get_if<WhileStmt>(&stmt.node)) {
        indent(level);
        out_ << "while (";
        print_expr(while_stmt->cond);
        out_ << ')';
        bool body_is_compound =
            std::holds_alternative<CompoundStmt>(ast_.stmt(while_stmt->body).node);
        print_controlled_body(while_stmt->body, level);
        if (body_is_compound)
            out_ << '\n';
        return;
    }

    if (auto* return_stmt = std::get_if<ReturnStmt>(&stmt.node)) {
        indent(level);
        out_ << "return";
        if (return_stmt->expr.valid()) {
            out_ << ' ';
            print_expr(return_stmt->expr);
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

    if (auto* goto_stmt = std::get_if<GotoStmt>(&stmt.node)) {
        indent(level);
        out_ << "goto ";
        if (goto_stmt->name)
            out_ << *goto_stmt->name;
        out_ << ";\n";
        return;
    }

    auto* label_stmt = std::get_if<LabelStmt>(&stmt.node);
    assert(label_stmt && "unknown statement kind");
    const LabelDecl& label = ast_.label(label_stmt->label);
    if (label.name)
        out_ << *label.name;
    out_ << ":\n";
    print_stmt(label_stmt->stmt, level);
}

void Printer::print() {
    const auto& declarations = ast_.translation_unit().decls;
    for (size_t i = 0; i < declarations.size(); ++i) {
        print_decl(declarations[i], 0);
        if (i + 1 < declarations.size())
            out_ << '\n';
    }
}

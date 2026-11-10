#include "Parser.hpp"

#include <cassert>
#include <utility>

Parser::Parser(AST& ast, std::vector<Token> tokens, Diagnostics& diagnostics)
    : ast_(ast), tokens_(std::move(tokens)), diagnostics_(diagnostics) {
    assert(!tokens_.empty() && "lexer must provide an EOF token");
}

const Token& Parser::current() const {
    assert(pos_ < tokens_.size());
    return tokens_[pos_];
}

const Token& Parser::peek_token(size_t offset) const {
    size_t index = pos_ + offset;
    if (index >= tokens_.size())
        return tokens_.back();
    return tokens_[index];
}

TokenKind Parser::peek_kind(size_t offset) const {
    return canonical_token_kind(peek_token(offset).kind);
}

SourceLoc Parser::loc() const {
    return current().loc;
}
bool Parser::at_end() const {
    return current().kind == TkEoi;
}

bool Parser::check(TokenKind kind) const {
    return canonical_token_kind(current().kind) == canonical_token_kind(kind);
}

bool Parser::accept(TokenKind kind) {
    if (!check(kind))
        return false;
    advance();
    return true;
}

void Parser::advance() {
    if (!at_end())
        ++pos_;
}

void Parser::expect(TokenKind kind, const char* what) {
    if (check(kind)) {
        advance();
        return;
    }
    if (at_end())
        error(loc(), std::string("expected ") + what + " before end of input");
    std::string got = current().text ? *current().text : "";
    error(loc(), std::string("expected ") + what + " but got '" + got + "'");
}

void Parser::error(SourceLoc location, const std::string& message) const {
    diagnostics_.error(location, message);
}

bool Parser::is_type_specifier_start() const {
    return check(TkVoid) || check(TkChar) || check(TkInt) || check(TkStruct);
}

bool Parser::is_declarator_start() const {
    return check(TkStar) || check(TkLparen) || check(TkIdentifier);
}

bool Parser::is_type_name_ahead() const {
    // current is '('
    TokenKind next = peek_kind(1);
    return next == TkVoid || next == TkChar || next == TkInt || next == TkStruct;
}

bool Parser::is_parameter_list_ahead() const {
    // current is '('; a parameter list follows if it is empty or begins with a
    // type specifier.  Otherwise this is a parenthesized declarator.
    TokenKind next = peek_kind(1);
    return next == TkRparen || next == TkVoid || next == TkChar || next == TkInt ||
           next == TkStruct;
}

TypeSpecId Parser::parse_type_spec() {
    SourceLoc start = loc();
    if (accept(TkVoid))
        return ast_.add_type_spec(TypeSpec{start, TypeSpec::Void{}});
    if (accept(TkChar))
        return ast_.add_type_spec(TypeSpec{start, TypeSpec::Char{}});
    if (accept(TkInt))
        return ast_.add_type_spec(TypeSpec{start, TypeSpec::Int{}});
    if (accept(TkStruct)) {
        TypeSpec::Struct spec;
        if (check(TkIdentifier)) {
            spec.tag = current().text;
            advance();
        }
        if (accept(TkLbrace)) {
            spec.has_braces = true;
            while (!check(TkRbrace)) {
                if (at_end())
                    error(loc(), "expected '}' in struct declaration");
                std::vector<DeclId> fields = parse_field_declaration();
                spec.fields.insert(spec.fields.end(), fields.begin(), fields.end());
            }
            expect(TkRbrace, "'}'");
        } else if (!spec.tag) {
            error(loc(), "expected struct tag or declaration");
        }
        return ast_.add_type_spec(TypeSpec{start, std::move(spec)});
    }
    error(start, "expected type specifier");
}

DeclaratorId Parser::parse_declarator(bool allow_name, bool allow_empty) {
    std::vector<SourceLoc> pointers;
    while (check(TkStar)) {
        pointers.push_back(loc());
        advance();
    }

    DeclaratorId direct;
    bool have_direct = false;

    if (allow_name && check(TkIdentifier)) {
        SourceLoc name_loc = loc();
        Symbol name = current().text;
        advance();
        direct = ast_.add_declarator(name_loc, NameDeclarator{name});
        have_direct = true;
    } else if (check(TkLparen)) {
        if (is_parameter_list_ahead()) {
            SourceLoc lparen = loc();
            bool explicit_void = false;
            std::vector<DeclId> params = parse_parameter_list(lparen, explicit_void);
            direct = ast_.add_declarator(
                lparen, FunctionDeclarator{DeclaratorId{}, std::move(params), explicit_void});
            have_direct = true;
        } else {
            advance(); // '('
            direct = parse_declarator(allow_name, true);
            expect(TkRparen, "')'");
            have_direct = true;
        }
    } else if (!allow_empty) {
        error(loc(), "expected declarator");
    }

    while (check(TkLparen)) {
        SourceLoc lparen = loc();
        bool explicit_void = false;
        std::vector<DeclId> params = parse_parameter_list(lparen, explicit_void);
        direct = ast_.add_declarator(lparen,
                                     FunctionDeclarator{direct, std::move(params), explicit_void});
        have_direct = true;
    }

    for (auto it = pointers.rbegin(); it != pointers.rend(); ++it)
        direct = ast_.add_declarator(*it, PointerDeclarator{direct});

    if (!have_direct && pointers.empty())
        return DeclaratorId{};
    return direct;
}

std::vector<DeclId> Parser::parse_parameter_list(SourceLoc lparen, bool& explicit_void) {
    (void)lparen;
    expect(TkLparen, "'('");
    std::vector<DeclId> params;
    if (accept(TkRparen))
        return params;
    if (check(TkVoid) && peek_kind(1) == TkRparen) {
        advance(); // void
        advance(); // )
        explicit_void = true;
        return params;
    }
    do {
        params.push_back(parse_parameter_declaration());
    } while (accept(TkComma));
    expect(TkRparen, "')' after parameter list");
    return params;
}

DeclId Parser::parse_parameter_declaration() {
    SourceLoc start = loc();
    TypeSpecId spec = parse_type_spec();
    DeclaratorId declarator = parse_declarator(true, true);
    return ast_.add_decl(start, ParamDecl{spec, declarator});
}

std::vector<DeclId> Parser::parse_field_declaration() {
    SourceLoc start = loc();
    TypeSpecId spec = parse_type_spec();
    if (!is_declarator_start())
        error(loc(), "expected struct member declarator");
    std::vector<DeclId> fields;
    do {
        DeclaratorId declarator = parse_declarator(true, false);
        if (!declarator_has_name(ast_, declarator))
            error(start, "expected struct member name");
        fields.push_back(ast_.add_decl(start, FieldDecl{spec, declarator}));
    } while (accept(TkComma));
    expect(TkSemicolon, "';' after struct member");
    return fields;
}

DeclId Parser::parse_external_declaration() {
    SourceLoc start = loc();
    TypeSpecId spec = parse_type_spec();

    if (!is_declarator_start()) {
        const TypeSpec& ts = ast_.type_spec(spec);
        if (std::holds_alternative<TypeSpec::Struct>(ts.node)) {
            expect(TkSemicolon, "';' after struct declaration");
            return ast_.add_decl(start, TagDecl{spec});
        }
        error(loc(), "expected declarator");
    }

    DeclaratorId declarator = parse_declarator(true, false);
    if (!declarator_has_name(ast_, declarator))
        error(start, "expected declarator name");

    if (check(TkLbrace)) {
        if (!declarator_produces_function(ast_, declarator))
            error(loc(), "function definition requires a function declarator");
        StmtId body = parse_compound_statement();
        return ast_.add_decl(start, FunctionDecl{spec, declarator, body});
    }

    expect(TkSemicolon, "';' after declaration");
    if (declarator_produces_function(ast_, declarator))
        return ast_.add_decl(start, FunctionDecl{spec, declarator, StmtId{}});
    return ast_.add_decl(start, VarDecl{spec, declarator});
}

DeclId Parser::parse_local_declaration() {
    SourceLoc start = loc();
    TypeSpecId spec = parse_type_spec();

    if (!is_declarator_start()) {
        const TypeSpec& ts = ast_.type_spec(spec);
        if (std::holds_alternative<TypeSpec::Struct>(ts.node)) {
            expect(TkSemicolon, "';' after struct declaration");
            return ast_.add_decl(start, TagDecl{spec});
        }
        error(loc(), "expected declarator");
    }

    DeclaratorId declarator = parse_declarator(true, false);
    if (!declarator_has_name(ast_, declarator))
        error(start, "expected declarator name");
    if (check(TkLbrace))
        error(loc(), "function definitions are only allowed at file scope");
    expect(TkSemicolon, "';' after declaration");

    if (declarator_produces_function(ast_, declarator))
        return ast_.add_decl(start, FunctionDecl{spec, declarator, StmtId{}});
    return ast_.add_decl(start, VarDecl{spec, declarator});
}

StmtId Parser::parse_statement() {
    SourceLoc start = loc();

    if (check(TkLbrace))
        return parse_compound_statement();

    if (accept(TkIf)) {
        expect(TkLparen, "'(' after if");
        ExprId cond = parse_expression();
        expect(TkRparen, "')' after if condition");
        StmtId then_stmt = parse_statement();
        StmtId else_stmt;
        if (accept(TkElse))
            else_stmt = parse_statement();
        return ast_.add_stmt(start, IfStmt{cond, then_stmt, else_stmt});
    }

    if (accept(TkWhile)) {
        expect(TkLparen, "'(' after while");
        ExprId cond = parse_expression();
        expect(TkRparen, "')' after while condition");
        StmtId body = parse_statement();
        return ast_.add_stmt(start, WhileStmt{cond, body});
    }

    if (accept(TkGoto)) {
        if (!check(TkIdentifier))
            error(loc(), "expected label name after goto");
        Symbol name = current().text;
        advance();
        expect(TkSemicolon, "';' after goto");
        return ast_.add_stmt(start, GotoStmt{name});
    }

    if (accept(TkContinue)) {
        expect(TkSemicolon, "';' after continue");
        return ast_.add_stmt(start, ContinueStmt{});
    }

    if (accept(TkBreak)) {
        expect(TkSemicolon, "';' after break");
        return ast_.add_stmt(start, BreakStmt{});
    }

    if (accept(TkReturn)) {
        ExprId expr;
        if (!check(TkSemicolon))
            expr = parse_expression();
        expect(TkSemicolon, "';' after return");
        return ast_.add_stmt(start, ReturnStmt{expr});
    }

    if (accept(TkSemicolon))
        return ast_.add_stmt(start, ExprStmt{ExprId{}});

    if (check(TkIdentifier) && peek_kind(1) == TkColon) {
        Symbol name = current().text;
        advance();
        advance(); // ':'
        LabelId label = ast_.add_label(start, name);
        StmtId body = parse_statement();
        return ast_.add_stmt(start, LabelStmt{label, body});
    }

    ExprId expr = parse_expression();
    expect(TkSemicolon, "';' after expression statement");
    return ast_.add_stmt(start, ExprStmt{expr});
}

StmtId Parser::parse_compound_statement() {
    SourceLoc start = loc();
    expect(TkLbrace, "'{'");
    std::vector<StmtId> items;
    while (!check(TkRbrace)) {
        if (at_end())
            error(loc(), "expected '}' before end of input");
        if (is_type_specifier_start())
            items.push_back(ast_.add_stmt(loc(), DeclStmt{parse_local_declaration()}));
        else
            items.push_back(parse_statement());
    }
    expect(TkRbrace, "'}'");
    return ast_.add_stmt(start, CompoundStmt{std::move(items)});
}

ExprId Parser::parse_expression() {
    ExprId lhs = parse_conditional();
    if (check(TkEqual)) {
        SourceLoc op_loc = loc();
        if (!is_assignment_lhs(lhs))
            error(op_loc, "left operand of assignment is not a unary expression");
        advance();
        ExprId rhs = parse_expression();
        return ast_.add_expr(op_loc, AssignExpr{lhs, rhs});
    }
    return lhs;
}

ExprId Parser::parse_conditional() {
    ExprId cond = parse_binary(1);
    if (check(TkQuestion)) {
        SourceLoc op_loc = loc();
        advance();
        ExprId then_expr = parse_expression();
        expect(TkColon, "':' in conditional expression");
        ExprId else_expr = parse_conditional();
        return ast_.add_expr(op_loc, ConditionalExpr{cond, then_expr, else_expr});
    }
    return cond;
}

int Parser::binary_precedence(TokenKind kind, BinaryExpr::Op& op) {
    switch (canonical_token_kind(kind)) {
        case TkPipepipe:
            op = BinaryExpr::Op::LogicalOr;
            return 1;
        case TkAmpamp:
            op = BinaryExpr::Op::LogicalAnd;
            return 2;
        case TkEqualequal:
            op = BinaryExpr::Op::Equal;
            return 3;
        case TkExclaimequal:
            op = BinaryExpr::Op::NotEqual;
            return 3;
        case TkLess:
            op = BinaryExpr::Op::Less;
            return 4;
        case TkPlus:
            op = BinaryExpr::Op::Add;
            return 5;
        case TkMinus:
            op = BinaryExpr::Op::Sub;
            return 5;
        case TkStar:
            op = BinaryExpr::Op::Mul;
            return 6;
        default:
            return 0;
    }
}

ExprId Parser::parse_binary(int min_precedence) {
    ExprId lhs = parse_unary();
    for (;;) {
        BinaryExpr::Op op;
        int precedence = binary_precedence(current().kind, op);
        if (precedence == 0 || precedence < min_precedence)
            break;
        SourceLoc op_loc = loc();
        advance();
        ExprId rhs = parse_binary(precedence + 1);
        lhs = ast_.add_expr(op_loc, BinaryExpr{op, lhs, rhs});
    }
    return lhs;
}

ExprId Parser::parse_unary() {
    if (check(TkAmp) || check(TkStar) || check(TkMinus) || check(TkExclaim)) {
        SourceLoc op_loc = loc();
        UnaryExpr::Op op = UnaryExpr::Op::Plus;
        switch (canonical_token_kind(current().kind)) {
            case TkAmp:
                op = UnaryExpr::Op::Addr;
                break;
            case TkStar:
                op = UnaryExpr::Op::Deref;
                break;
            case TkMinus:
                op = UnaryExpr::Op::Minus;
                break;
            case TkExclaim:
                op = UnaryExpr::Op::Not;
                break;
            default:
                break;
        }
        advance();
        ExprId operand = parse_unary();
        return ast_.add_expr(op_loc, UnaryExpr{op, operand});
    }

    if (check(TkSizeof)) {
        SourceLoc sizeof_loc = loc();
        advance();
        if (check(TkLparen) && is_type_name_ahead()) {
            advance(); // '('
            TypeSpecId spec = parse_type_spec();
            DeclaratorId declarator = parse_declarator(false, true);
            expect(TkRparen, "')' after type name in sizeof");
            return ast_.add_expr(sizeof_loc, SizeofExpr{SizeofExpr::TypeName{spec, declarator}});
        }
        ExprId operand = parse_unary();
        return ast_.add_expr(sizeof_loc, SizeofExpr{operand});
    }

    return parse_postfix();
}

ExprId Parser::parse_postfix() {
    ExprId expr = parse_primary();
    for (;;) {
        if (check(TkLsquare)) {
            SourceLoc op_loc = loc();
            advance();
            ExprId index = parse_expression();
            expect(TkRsquare, "']' after subscript");
            expr = ast_.add_expr(op_loc, SubscriptExpr{expr, index});
            continue;
        }
        if (check(TkLparen)) {
            SourceLoc op_loc = loc();
            advance();
            std::vector<ExprId> args;
            if (!check(TkRparen)) {
                do {
                    args.push_back(parse_expression());
                } while (accept(TkComma));
            }
            expect(TkRparen, "')' after argument list");
            expr = ast_.add_expr(op_loc, CallExpr{expr, std::move(args)});
            continue;
        }
        if (check(TkPeriod) || check(TkArrow)) {
            bool arrow = check(TkArrow);
            SourceLoc op_loc = loc();
            advance();
            if (!check(TkIdentifier))
                error(loc(), "expected member name");
            Symbol member = current().text;
            advance();
            expr = ast_.add_expr(op_loc, MemberExpr{expr, member, arrow});
            continue;
        }
        break;
    }
    return expr;
}

ExprId Parser::parse_primary() {
    SourceLoc start = loc();
    if (check(TkIdentifier)) {
        Symbol name = current().text;
        advance();
        return ast_.add_expr(start, IdentifierExpr{name});
    }
    if (check(TkConstant)) {
        Symbol spelling = current().text;
        advance();
        return ast_.add_expr(start, ConstantExpr{spelling});
    }
    if (check(TkStringLiteral)) {
        Symbol spelling = current().text;
        advance();
        return ast_.add_expr(start, StringLiteralExpr{spelling});
    }
    if (check(TkLparen)) {
        advance();
        ExprId expr = parse_expression();
        expect(TkRparen, "')'");
        return expr; // parentheses are not represented in the AST
    }
    error(start, "expected expression");
}

bool Parser::is_assignment_lhs(ExprId id) const {
    const Expr& expr = ast_.expr(id);
    return !std::holds_alternative<BinaryExpr>(expr.node) &&
           !std::holds_alternative<ConditionalExpr>(expr.node) &&
           !std::holds_alternative<AssignExpr>(expr.node);
}

void Parser::parse_translation_unit() {
    if (at_end())
        error(loc(), "expected external declaration");
    while (!at_end())
        ast_.translation_unit().decls.push_back(parse_external_declaration());
}

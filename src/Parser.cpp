#include "Parser.hpp"

#include "Diagnostic.hpp"
#include "Token.hpp"

SpecDecl* Parser::parseSpecDecl(DeclKind dKind) {
    Specifier* spec = nullptr;
    Locatable loc(getLoc());
    if (accept(TK_VOID)) {
        spec = new VoidSpecifier(loc);
    } else if (accept(TK_CHAR)) {
        spec = new CharSpecifier(loc);
    } else if (accept(TK_INT)) {
        spec = new IntSpecifier(loc);
    } else if (accept(TK_STRUCT)) {
        Symbol tag = nullptr;
        if (check(TK_IDENTIFIER)) {
            tag = peekToken().Text;
            popToken();
        } else if (!check(TK_LBRACE)) {
            errorloc(getLoc(), "Expected struct declaration list but got `",
                     *peekToken().Text, "'");
        }
        auto* structSpec = new StructSpecifier(loc, tag);

        if (accept(TK_LBRACE)) {
            do {  // parse struct member declarations
                auto* specDecl = parseSpecDecl(DeclKind::CONCRETE);
                structSpec->addComponent(specDecl);
                expect(TK_SEMICOLON, ";");
            } while (!accept(TK_RBRACE));
        }
        spec = structSpec;

    } else {
        errorloc(loc, "Expected type specifier but got `", *peekToken().Text,
                 "'");
    }

    Declarator* decl = parseDeclarator();

    if (!decl->isEmptyDeclarator()) {
        // At all places where non-abstractness is required, declarators are
        // also optional in the grammar, therefore it's okay to accept a simple
        // empty primitive declarator here anyway.

        // TODO check if decl is a valid declarator or abstract declarator and
        //      that it fits with what is required by dKind
        if((decl->kind != DeclKind::ANY) && (dKind != decl->kind)) {
            errorloc(loc, "Expected different DeclKind");
        }
    }

    return new SpecDecl(getLoc(), spec, decl);
}

Declarator* Parser::parseNonFunDeclarator(void) {
    switch (peekToken().Kind) {
        case TK_LPAREN: {
            if (checkLookAhead(TK_RPAREN) || checkLookAhead(TK_VOID) ||
                checkLookAhead(TK_CHAR) || checkLookAhead(TK_INT) ||
                checkLookAhead(TK_STRUCT)) {
                // this has to be an abstract function declarator, not a
                // parenthesized declarator, so we add an empty primitive
                // declarator
                return new PrimitiveDeclarator(getLoc(), nullptr, DeclKind::ABSTRACT);
            }
            popToken();
            auto* res = parseDeclarator();
            expect(TK_RPAREN, ")");
            return res;
        }

        case TK_STAR: {
            Locatable loc(getLoc());
            popToken();
            auto* inner = parseDeclarator();
            return new PointerDeclarator(loc, inner);
        }

        case TK_IDENTIFIER: {
            auto* res = new PrimitiveDeclarator(getLoc(), peekToken().Text);
            popToken();
            return res;
        }
        default:
            return new PrimitiveDeclarator(getLoc(), nullptr);
    }
}

Declarator* Parser::parseDeclarator(void) {
    Declarator* res = parseNonFunDeclarator();
    while (check(TK_LPAREN)) {
        auto* funDecl = new FunctionDeclarator(getLoc(), res);
        res = funDecl;
        popToken();

        if (accept(TK_RPAREN)) {
            continue;
        }

        do {
            auto* param = parseSpecDecl(DeclKind::ANY);
            funDecl->addParameter(param);
        } while (accept(TK_COMMA));

        expect(TK_RPAREN, ")");
    }
    return res;
}

Parser::Parser(std::queue<Token *> tokens) {
    this->currTok = tokens.front();
    tokens.pop();
    this->tokens = tokens;
}

void Parser::popToken(void) {
    this->currTok = tokens.front();
    this->tokens.pop();
}

const Token &Parser::peekToken(void) {
    return *(this->tokens.front());
}

const Locatable &Parser::getLoc(void) {
    return Locatable(*(this->currTok));
}

void Parser::expect(TokenKind tk, const char *txt) {
    if ((*currTok).Kind == tk){
        popToken();
    } else {
        errorloc(getLoc(), "Expected " + *txt + *" but got '", *currTok->Text, "'");
    }
}

bool Parser::accept(TokenKind tk) { 
    if ((*currTok).Kind == tk){
        popToken();
        return true;
    } else {
        return false;
    } 
}

bool Parser::check(TokenKind tk) {
    return ((*currTok).Kind == tk);
}

bool Parser::checkLookAhead(TokenKind tk) {
    return (peekToken().Kind == tk);
}

void StructSpecifier::addComponent(SpecDecl* SpecDecl) {
    structDeclarationList.push(SpecDecl);
}

ExternalDeclaration::ExternalDeclaration(Locatable loc, SpecDecl* sD) : Locatable(loc) {
    this->specDecl = sD;
}

ExternalDeclaration::ExternalDeclaration(Locatable loc, SpecDecl *sD,
                                         CompoundStatement *c) : Locatable(loc) {
    this->specDecl = sD;
    this->CompState = c;
}


AST::AST(Locatable loc) : Locatable(loc) {}

void AST::addExternalDeclaration(ExternalDeclaration *exDecl) {
    this->externalDeclarationList.push(exDecl);
}

Statement::Statement(Locatable loc) : Locatable(loc) {}

IfStatement::IfStatement(Locatable loc, Expression* ex, Statement* ifSt, Statement* elseSt) : Statement(loc) {
    this->exp = ex;
    this->ifStatement = ifSt;
    this->elseStatement = elseSt;
}

WhileStatement::WhileStatement(Locatable loc, Expression* expr, Statement* stat) : Statement(loc) {
    this->expression = expr;
    this->statement = stat;
}

GoToStatement::GoToStatement(Locatable loc, const Token* tok) : Statement(loc) {
    this->token = tok;
}

ExpressionStatement::ExpressionStatement(Locatable loc) : Statement(loc) {}

ExpressionStatement::ExpressionStatement(Locatable loc, Expression *e) : Statement(loc) {
    this->expr = e;
}

IdentifierStatement::IdentifierStatement(Locatable loc, PrimaryExpression* e, Statement* s) : Statement(loc) {
    this->id = e;
    this->statement = s;
}

SimpleStatement::SimpleStatement(Locatable loc, StatementType t) : Statement(loc) {
    this->type = t;
}

CompoundStatement::CompoundStatement(Locatable loc) : Statement(loc) {}

void CompoundStatement::addBlockItem(Locatable* loc) {
    this->blockItemList.push(loc);
}

Expression::Expression(Locatable loc) : Locatable(loc) {}

Expression::Expression(Locatable loc, Expression* c) : Locatable(loc) {
    this->conEx = c;
}

Expression::Expression(Locatable loc, Expression* c, Expression* e) : Locatable(loc) {
    this->conEx = c;
    this->Expr = e;
}

ConditionalExpression::ConditionalExpression(Locatable loc) : Expression(loc){}

ConditionalExpression::ConditionalExpression(Locatable loc,
                                             ConditionalExpression *c,
                                             ConditionalExpression *b)  : Expression(loc){
                                                this->conEx = c;
    this->biExpr = b;
                                             }

BinaryExpression::BinaryExpression(Locatable loc) : ConditionalExpression(loc) {}

BinaryExpression::BinaryExpression(Locatable loc,BinaryExpression *u,
                                   BinaryExpression *b, TokenKind o) : ConditionalExpression(loc) {
                                    this->biExpr = b;
                                    this->op = o;
                                    this->unExpr = u;
                                   }

UnaryExpression::UnaryExpression(Locatable loc) : BinaryExpression(loc) {}

UnaryExpression::UnaryExpression(Locatable loc, UnaryExpression *u) : BinaryExpression(loc) {
    this->unExpr = u;
}

UnaryExpression::UnaryExpression(Locatable loc, UnaryExpression *u,
                                 TokenKind tok) : BinaryExpression(loc) {
                                    this->unExpr = u;
                                    this->op = tok;
                                 }

PostFixExpression::PostFixExpression(Locatable loc) : UnaryExpression(loc) {}

PostFixExpression::PostFixExpression(Locatable loc, Expression *e) : UnaryExpression(loc) {
    this->expr = e;
}

PostFixExpression::PostFixExpression(Locatable loc, AssignmentExpression *a) : UnaryExpression(loc) {
    this->ass = a;
}
PostFixExpression::PostFixExpression(Locatable loc, const Token *tok) : UnaryExpression(loc) {
    this->token = tok;
}

AssignmentExpression::AssignmentExpression(Locatable loc, UnaryExpression *u,
                                           ConditionalExpression *c)
    : Expression(loc) {
    this->unExpr = u;
    this->condExpr = c;
}

PrimaryExpression::PrimaryExpression(Locatable loc, Expression* e) : PostFixExpression(loc) {
        this->expr = e;
}

PrimaryExpression::PrimaryExpression(Locatable loc, const Token* tok) : PostFixExpression(loc) {
    this->token = tok;
}

Declarator::Declarator(Locatable loc) : Locatable(loc) {}

Declarator::Declarator(Locatable loc, Declarator *decl) : Locatable(loc) {
    this->Decl = decl;
}

Declarator::Declarator(Locatable loc, DeclKind kind) : Locatable(loc) {
    this->kind = kind;
}

Specifier::Specifier(Locatable loc) : Locatable(loc) {}

SpecDecl::SpecDecl(Locatable loc, Specifier* spec, Declarator* decl) : Locatable(loc) {
    this->spec = spec;
    this->decl = decl;
}

FunctionDeclarator::FunctionDeclarator(Locatable loc, Declarator *decl) : Declarator(loc, decl) {}

void FunctionDeclarator::addParameter(SpecDecl *SpecDecl) {
    parameterList.push(SpecDecl);
}

PointerDeclarator::PointerDeclarator(Locatable loc, Declarator *Decl) : Declarator(loc, Decl) {}

PrimitiveDeclarator::PrimitiveDeclarator(Locatable loc, Symbol Sym) : Declarator(loc) {
    this->Sym = Sym;
}

PrimitiveDeclarator::PrimitiveDeclarator(Locatable loc, Symbol Sym,
                                         DeclKind dKind) : Declarator(loc,dKind) {
                                            this->Sym = Sym;
                                         }

bool Declarator::isEmptyDeclarator() { return (Decl == nullptr); }

VoidSpecifier::VoidSpecifier(Locatable loc) : Specifier(loc){};

IntSpecifier::IntSpecifier(Locatable loc) : Specifier(loc){};

CharSpecifier::CharSpecifier(Locatable loc) : Specifier(loc){};

StructSpecifier::StructSpecifier(Locatable loc, Symbol sym) : Specifier(loc), Sym(sym){};

AST *Parser::parse() { 
    auto ast = new AST(getLoc());
    while (!tokens.empty()){
        auto exDecl = parseExternalDeclaration();
        ast->addExternalDeclaration(exDecl);
    }
    return ast; 
}

ExternalDeclaration *Parser::parseExternalDeclaration() { 
    SpecDecl* specDecl = parseSpecDecl(DeclKind::ANY);
    if (check(TK_LBRACE)) {
        CompoundStatement* comp = parseCompoundStatement();
        return new ExternalDeclaration(getLoc(), specDecl, comp);
    }
    return new ExternalDeclaration(getLoc(), specDecl); 
}

Statement *Parser::parseStatement() { 
    if (check(TK_LBRACE)) {
        return parseCompoundStatement();
    } else if (check(TK_IF)) {
        popToken();
        expect(TK_LPAREN, "(");
        auto expression = parseExpression();
        expect(TK_RPAREN, ")");
        auto ifStatement = parseStatement();
        Statement* elseStatement;
        if (check(TK_ELSE)) {
            popToken();
            elseStatement = parseStatement();
        } else {
            elseStatement = nullptr;
        }
        return new IfStatement(getLoc(), expression, ifStatement, elseStatement);
    
    } else if (check(TK_WHILE)) {
        popToken();
        expect(TK_LPAREN,"(");
        auto expression = parseExpression();
        expect(TK_RPAREN,")");
        auto statement = parseStatement();
        return new WhileStatement(getLoc(), expression, statement);

    } else if (check(TK_GOTO)) {
        popToken();
        PrimaryExpression* p = parsePrimaryExpression();
        if (p->token->Kind == TokenKind::TK_IDENTIFIER){
            return new GoToStatement(getLoc(),p->token);
        } else {
            errorloc(getLoc(), "Expected identifier, but got", p->token);
        }
    } else if (check(TK_CONTINUE)) {
        popToken();
        return new SimpleStatement(getLoc(),StatementType::CONTINUE);
    } else if (check(TK_BREAK)) {
        popToken();
        return new SimpleStatement(getLoc(),StatementType::BREAK);
    } else {
        Expression* e = nullptr;
        if (check(TK_SEMICOLON)) {
            popToken();
            return new ExpressionStatement(getLoc());
        } else {
            if (check(TK_IDENTIFIER)) {
                PrimaryExpression* p = parsePrimaryExpression();
                if (check(TK_EQUALEQUAL)) {
                    Statement* s = parseStatement();
                    return new IdentifierStatement(getLoc(), p, s);
                } else {
                    errorloc(getLoc(), "Expected '==', but got", peekToken());
                }
            } else {
                e = parseExpression();
                if (check(TK_SEMICOLON)) {
                    return new ExpressionStatement(getLoc(),e);
                } else {
                    errorloc(getLoc(), "Expected ';', but got", peekToken());
                }
            }
            
        }
    }
}

CompoundStatement *Parser::parseCompoundStatement() { 
    if (accept(TK_LBRACE)) {
        CompoundStatement* comp = new CompoundStatement(getLoc());
        while (!check(TK_RBRACE)) {
            if (check(TK_VOID) || check(TK_CHAR) || 
            check(TK_INT) || check(TK_STRUCT)) {
                comp->addBlockItem(parseSpecDecl(DeclKind::ANY));
            } else {
                comp->addBlockItem(parseStatement());
            }
        } 
        popToken();
        return comp;
    } else {
        errorloc(getLoc(), "Expected '{', but got '", *currTok->Text, "'");
    }
    return nullptr; 
}

PrimaryExpression *Parser::parsePrimaryExpression() { 
    if (check(TK_LPAREN)) {
        popToken();
        Expression* e = parseExpression();
        expect(TK_RPAREN,")");
        return new PrimaryExpression(getLoc(), e);
    } else {
        if (check(TK_IDENTIFIER) || check(TK_CONSTANT) ||check(TK_STRING_LITERAL)) {
            const Token* tok = & peekToken();
            popToken();
            return new PrimaryExpression(getLoc(), tok);
        } else {
            errorloc(getLoc(), "Expected id, constant or string-literal, but got ", peekToken());
        }
    } 
}

PostFixExpression *Parser::parsePostFixExpression() { 
    PrimaryExpression* p = parsePrimaryExpression();
    if (check(TK_LBRACE)) {
        popToken();
        Expression* e = parseExpression();
        expect(TK_RBRACE,"]");
        return new PostFixExpression(getLoc(),e);
    } else if (check(TK_LPAREN)) {
        popToken();
        if (!check(TK_RPAREN)) {
            UnaryExpression* u = parseUnaryExpression();
            expect(TK_EQUAL,"=");
            ConditionalExpression* c = parseConditionalExpression();
            expect(TK_RPAREN,")");
            AssignmentExpression a = AssignmentExpression(getLoc(),u,c);
            return new PostFixExpression(getLoc(), &a);
        }
        expect(TK_RPAREN,")");
        return new PostFixExpression(getLoc());
    } else if (check(TK_PERIOD) || check(TK_ARROW)) {
        popToken();
        PrimaryExpression* p = parsePrimaryExpression();
        if (p->token->Kind == TokenKind::TK_IDENTIFIER){
            return new PostFixExpression(getLoc(),p->token);
        } else {
            errorloc(getLoc(), "Expected identifier, but got", p->token);
        }
    } else {
        return p;
    } 
}

UnaryExpression *Parser::parseUnaryExpression() { 
    if (check(TK_SIZEOF) || check(TK_AMP) || check(TK_STAR) || check(TK_MINUS) || check(TK_EXCLAIM)) {
        Token token = peekToken();
        popToken();
        UnaryExpression* u = parseUnaryExpression();
        return new UnaryExpression(getLoc(), u, token.Kind);
    } else {
        return parsePostFixExpression(); 
    }
}

BinaryExpression *Parser::parseBinaryExpression() { 
    UnaryExpression* u = parseUnaryExpression();
    if (check(TK_EOI)){
       return u; 
    } else if (check(TK_STAR) || check(TK_PLUS) || check(TK_MINUS) || check(TK_LESS) || check(TK_EQUALEQUAL) || check(TK_EXCLAIMEQUAL) || check(TK_AMPAMP) || check(TK_PIPEPIPE)) {
        TokenKind o = peekToken().Kind;
        popToken();
        BinaryExpression* b = parseBinaryExpression();
        return new BinaryExpression(getLoc(),u,b,o);
    } else {
        errorloc(getLoc(), "Expected End of Input, but got ", peekToken());
    }
}

ConditionalExpression *Parser::parseConditionalExpression() { 
    BinaryExpression* b = parseBinaryExpression();
    if (check(TK_QUESTION)){
        popToken();
        Expression* parseExpression();
        if (check(TK_COLON)) {
            popToken();
            ConditionalExpression* c = parseConditionalExpression();
            return new ConditionalExpression(getLoc(),b,c);
        } else{
            errorloc(getLoc(), "Expected ':', but got", peekToken());
        }
    } else {
        return b; 
    }
}

Expression *Parser::parseExpression() { 
    ConditionalExpression* c = parseConditionalExpression();
    if (check(TK_EQUAL)) {
        popToken();
        UnaryExpression* u = dynamic_cast<UnaryExpression*> (c);
        if (u != NULL) {
            Expression* e = parseExpression();
            return new Expression(getLoc(),u,e);
        } else {
            errorloc(getLoc(),"Expected Unary Expression, but got ", c);
        }
    } else {
        return c; 
    }
}

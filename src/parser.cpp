#include "parser.h"

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

    return new SpecDecl(spec, decl);
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

ExternalDeclaration::ExternalDeclaration(Locatable loc) : Locatable(loc) {}

Statement::Statement(Locatable loc) : Locatable(loc) {}

IfStatement::IfStatement(Locatable loc) : Statement(loc) {}

WhileStatement::WhileStatement(Locatable loc) : Statement(loc) {}

GoToStatement::GoToStatement(Locatable loc) : Statement(loc) {}

ExpressionStatement::ExpressionStatement(Locatable loc) : Statement(loc) {}

IdentifierStatement::IdentifierStatement(Locatable loc) : Statement(loc) {}

SimpleStatement::SimpleStatement(Locatable loc) : Statement(loc) {}

CompoundStatement::CompoundStatement(Locatable loc) : Statement(loc) {}

BlockItemList::BlockItemList(Locatable loc) : Locatable(loc) {}

Expression::Expression(Locatable loc) : Locatable(loc) {}

ConditionalExpression::ConditionalExpression(Locatable loc) : Expression(loc){}

BinaryExpression::BinaryExpression(Locatable loc) : Expression(loc) {}

UnaryExpression::UnaryExpression(Locatable loc) : Expression(loc) {}

PostFixExpression::PostFixExpression(Locatable loc) : Expression(loc) {}

PrimaryExpression::PrimaryExpression(Locatable loc) : Expression(loc) {}

Declarator::Declarator(Locatable loc) : Locatable(loc) {}

Declarator::Declarator(Locatable loc, Declarator *decl) : Locatable(loc) {
    this->Decl = decl;
}

Declarator::Declarator(Locatable loc, DeclKind kind) : Locatable(loc) {
    this->kind = kind;
}

Specifier::Specifier(Locatable loc) : Locatable(loc) {}

SpecDecl::SpecDecl(Specifier* spec, Declarator* decl){
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

                                         }

bool Declarator::isEmptyDeclarator() { return (Decl == nullptr); }

VoidSpecifier::VoidSpecifier(Locatable loc) : Specifier(loc){};

IntSpecifier::IntSpecifier(Locatable loc) : Specifier(loc){};

CharSpecifier::CharSpecifier(Locatable loc) : Specifier(loc){};

StructSpecifier::StructSpecifier(Locatable loc, Symbol sym) : Specifier(loc), Sym(sym){};

AST *Parser::parse() { return nullptr; }

ExternalDeclaration *Parser::parseExternalDeclaration() { return nullptr; }

CompoundStatement *Parser::parseCompoundStatement() { return nullptr; }

BlockItemList *Parser::parseBlockItemList() { return nullptr; }

Statement *Parser::parseStatement() { return nullptr; }

Expression *Parser::parseExpression() { return nullptr; }

ConditionalExpression *Parser::parseConditionalExpression() { return nullptr; }

BinaryExpression *Parser::parseBinaryExpression() { return nullptr; }

UnaryExpression *Parser::parseUnaryExpression() { return nullptr; }

PostFixExpression *Parser::parsePostFixExpression() { return nullptr; }

PrimaryExpression *Parser::parsePrimaryExpression() { return nullptr; }

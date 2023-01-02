#pragma once

#include <queue>
#include <typeinfo>

#include "Diagnostic.hpp"
#include "Token.hpp"

// Declarators and abstract declarators share a lot of syntactical and
// semantical properties. Therefore, we use the same AST data structures to
// represent both. If at some place, a specific kind of declarators is required
// (by giving one of the below `DeclKind`s), we need to check separately whether
// the declarator satisfies this requirement.
enum class DeclKind { ANY, ABSTRACT, CONCRETE };

class Declarator : public Locatable{
    public:
    Declarator* Decl;
    DeclKind kind = DeclKind::ANY;
    Declarator(Locatable loc);
    Declarator(Locatable loc, Declarator* decl);
    Declarator(Locatable loc, DeclKind kind);
    bool isEmptyDeclarator();
    private:
};

class Specifier : public Locatable {
    public:
    Specifier(Locatable loc);
    private:
};

class SpecDecl : public Locatable {
    public:
    Specifier* spec;
    Declarator* decl;
    SpecDecl(Locatable loc, Specifier *spec, Declarator *decl);
    private:
};

class PrimitiveDeclarator : public Declarator {
    public:
    Symbol Sym;
    const std::string Str;
    PrimitiveDeclarator(Locatable loc, Symbol Sym);
    PrimitiveDeclarator(Locatable loc, Symbol Sym, DeclKind dKind);

    private:
};

class PointerDeclarator : public Declarator {
    public:
    PointerDeclarator(Locatable loc, Declarator* decl);
    private:
};

class FunctionDeclarator : public Declarator {
    public:
    std::queue<SpecDecl*> parameterList;
    FunctionDeclarator(Locatable loc, Declarator* decl);
    void addParameter(SpecDecl* SpecDecl);
    private:
};

class VoidSpecifier : public Specifier {
    public:
    VoidSpecifier(Locatable loc);
    private:
};

class IntSpecifier : public Specifier {
    public:
    IntSpecifier(Locatable loc);
    private:
};

class CharSpecifier : public Specifier {
    public:
    CharSpecifier(Locatable loc);
    private:
};

class StructSpecifier : public Specifier {
    public:
    Symbol Sym;
    std::queue<SpecDecl*> structDeclarationList;
    StructSpecifier(Locatable loc, Symbol sym);
    void addComponent(SpecDecl* SpecDecl);
    private:
};

class Expression : public Locatable {
    public:
    Expression* conEx;
    Expression* Expr;
    Expression(Locatable loc);
    Expression(Locatable loc, Expression* c);
    Expression(Locatable loc, Expression* c, Expression* e);
    private:
};

class ConditionalExpression : public Expression {
    public:
    ConditionalExpression* conEx;
    ConditionalExpression* biExpr;
    ConditionalExpression(Locatable loc);
    ConditionalExpression(Locatable loc, ConditionalExpression* c, ConditionalExpression* b);
    private:
};

class BinaryExpression : public ConditionalExpression {
    public:
    BinaryExpression* unExpr;
    BinaryExpression* biExpr;
    TokenKind op;
    BinaryExpression(Locatable loc);
    BinaryExpression(Locatable loc,BinaryExpression* u, BinaryExpression* b, TokenKind o);
    private:
};

class UnaryExpression : public BinaryExpression {
    public:
    UnaryExpression* unExpr;
    TokenKind op;
    UnaryExpression(Locatable loc);
    UnaryExpression(Locatable loc, UnaryExpression* u);
    UnaryExpression(Locatable loc, UnaryExpression* u, TokenKind tok);
    private:
};

class AssignmentExpression : public Expression {
    public:
    UnaryExpression* unExpr;
    ConditionalExpression* condExpr;
    AssignmentExpression(Locatable loc, UnaryExpression* u, ConditionalExpression* c);
    private:
};

class PostFixExpression : public UnaryExpression {
    public:
    Expression* expr;
    AssignmentExpression* ass;
    const Token* token;
    PostFixExpression(Locatable loc);
    PostFixExpression(Locatable loc, Expression* e);
    PostFixExpression(Locatable loc, AssignmentExpression* ass);
    PostFixExpression(Locatable loc, const Token* tok);
    private:
};

class PrimaryExpression : public PostFixExpression {
    public:
    const Token* token = nullptr;
    Expression* expr = nullptr;
    PrimaryExpression(Locatable loc, Expression* e);
    PrimaryExpression(Locatable loc, const Token* tok);
    private:
};

class Statement : public Locatable {
    public:
    Statement(Locatable loc);
    private:
};

class CompoundStatement : public Statement {
    public:
    std::queue<Locatable*> blockItemList;
    CompoundStatement(Locatable loc);
    void addBlockItem(Locatable* loc);
    private:
};

class IfStatement : public Statement {
    public:
    Expression* exp;
    Statement* ifStatement;
    Statement* elseStatement;
    IfStatement(Locatable loc, Expression* expr, Statement* ifSt, Statement* elseSt);
    private:
};

class WhileStatement : public Statement {
    public:
    Expression* expression;
    Statement* statement;
    WhileStatement(Locatable loc, Expression* expr, Statement* stat);
    private:
};

class GoToStatement : public Statement {
    public:
    const Token* token;
    GoToStatement(Locatable loc, const Token* tok);
    private:
};

class ExpressionStatement : public Statement {
    public:
    Expression* expr;
    ExpressionStatement(Locatable loc);
    ExpressionStatement(Locatable loc, Expression* e);
    private:
};

class IdentifierStatement : public Statement {
    public:
    PrimaryExpression* id;
    Statement* statement;
    IdentifierStatement(Locatable loc, PrimaryExpression* e, Statement* s);
    private:
};

enum class StatementType {CONTINUE, BREAK};

class SimpleStatement : public Statement {
    public:
    StatementType type;
    SimpleStatement(Locatable loc, StatementType t);
    private:
};

class ExternalDeclaration : public Locatable {
    public:
    SpecDecl* specDecl;
    CompoundStatement* CompState;
    ExternalDeclaration(Locatable loc, SpecDecl* sD);
    ExternalDeclaration(Locatable loc, SpecDecl* sD, CompoundStatement* c);
    private:
};

class AST : public Locatable {
    public:
    std::queue<ExternalDeclaration*> externalDeclarationList;
    AST(Locatable loc);
    void addExternalDeclaration(ExternalDeclaration* exDecl);
    private:
    
};

/// This class should implement the parser that translates a stream of tokens
/// into an abstract syntax tree.
class Parser {
   public:
   Parser(std::queue<Token*> tokens);

    AST* parse();

   private:
    Token* currTok;
    std::queue<Token*> tokens;

    /// Advance the parsing state to the next token.
    void popToken(void);

    /// Return (a reference to) the current token.
    const Token& peekToken(void);

    /// Return the location of the current token.
    const Locatable& getLoc(void);

    /// Check if the current token has TokenKind `tk`. If it has, advance the
    /// parsing state to the next token, otherwise note an error that `txt` was
    /// expected but something else encountered.
    void expect(TokenKind tk, const char* txt);

    /// Check if the current token has TokenKind `tk`. If it has, advance the
    /// parsing state to the next token and return true. Otherwise, return
    /// false and keep the parsing state unchanged.
    bool accept(TokenKind tk);

    /// Check if the current token has TokenKind `tk`. If it has, return true.
    /// Otherwise, return false. Keep the parsing state unchanged in both cases.
    bool check(TokenKind tk);

    /// Check if the token after the current token has TokenKind `tk` and
    /// return true iff this is the case. Keep the parsing state unchanged in
    /// both cases.
    bool checkLookAhead(TokenKind tk);

    /// Internal methods for use in parseSpecDecl()
    Declarator* parseDeclarator(void);
    Declarator* parseNonFunDeclarator(void);

    /// Parse a type specifier followed by an abstract or non-abstract
    /// declarator (i.e. a typename or a declaration).
    /// The current token is expected to be the first token of the type
    /// specifier
    ///
    /// If `dKind` is `DeclKind::ABSTRACT`: verify that the declarator is a
    /// valid abstract declarator.
    /// If `dKind` is `DeclKind::CONCRETE`: verify that the declarator is a
    /// valid non-abstract declarator.
    /// If `dKind` is `DeclKind::ANY`: do not verify abstractness.
    SpecDecl* parseSpecDecl(DeclKind dKind);

    ExternalDeclaration* parseExternalDeclaration();

    CompoundStatement* parseCompoundStatement();

    Statement* parseStatement();

    Expression* parseExpression();

    ConditionalExpression* parseConditionalExpression();

    BinaryExpression* parseBinaryExpression();

    UnaryExpression* parseUnaryExpression();

    PostFixExpression* parsePostFixExpression();

    PrimaryExpression* parsePrimaryExpression();
};
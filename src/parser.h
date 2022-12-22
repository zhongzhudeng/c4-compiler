#pragma once

#include <queue>

#include "Diagnostic.hpp"
#include "Token.hpp"

// Declarators and abstract declarators share a lot of syntactical and
// semantical properties. Therefore, we use the same AST data structures to
// represent both. If at some place, a specific kind of declarators is required
// (by giving one of the below `DeclKind`s), we need to check separately whether
// the declarator satisfies this requirement.
enum class DeclKind { ANY, ABSTRACT, CONCRETE };

class AST : public Locatable {
    public:
    private:
    
};

class ExternalDeclaration : public Locatable {
    public:
    ExternalDeclaration(Locatable loc);
    private:
};

class Statement : public Locatable {
    public:
    Statement(Locatable loc);
    private:
};

class CompoundStatement : public Statement {
    public:
    CompoundStatement(Locatable loc);
    private:
};

class IfStatement : public Statement {
    public:
    IfStatement(Locatable loc);
    private:
};

class WhileStatement : public Statement {
    public:
    WhileStatement(Locatable loc);
    private:
};

class GoToStatement : public Statement {
    public:
    GoToStatement(Locatable loc);
    private:
};

class ExpressionStatement : public Statement {
    public:
    ExpressionStatement(Locatable loc);
    private:
};

class IdentifierStatement : public Statement {
    public:
    IdentifierStatement(Locatable loc);
    private:
};

class SimpleStatement : public Statement {
    public:
    SimpleStatement(Locatable loc);
    private:
};

enum class StatementType {CONTINUE, BREAK};

class BlockItemList : Locatable {
    public:
    BlockItemList(Locatable loc);
    private:
};

class Expression : public Locatable {
    public:
    Expression(Locatable loc);
    private:
};

class ConditionalExpression : public Expression {
    public:
    ConditionalExpression(Locatable loc);
    private:
};

class BinaryExpression : public Expression {
    public:
    BinaryExpression(Locatable loc);
    private:
};

class UnaryExpression : public Expression {
    public:
    UnaryExpression(Locatable loc);
    private:
};

class PostFixExpression : public Expression {
    public:
    PostFixExpression(Locatable loc);
    private:
};

class PrimaryExpression : public Expression {
    public:
    PrimaryExpression(Locatable loc);
    private:
};

enum class BinaryOperator {STAR,PLUS,MINUS,LESS,EQUALEQUAL,EXCLAIMEQUAL,AMPAMP,PIPEPIPE};

enum class UnaryOperator {AMP,STAR,MINUS,EXCLAIM,SIZEOF};

enum class PostFixOperator {SQUARE,PAREN,ARROW,DOT};

enum class PrimaryOperator {PAREN,CONSTANT,STRING_LITERAL,IDENTIFIER};

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

class SpecDecl{
    public:
    Specifier* spec;
    Declarator* decl;
    SpecDecl(Specifier *spec, Declarator *decl);
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


/// This class should implement the parser that translates a stream of tokens
/// into an abstract syntax tree.
class Parser {
   public:
   Parser(std::queue<Token*> tokens);

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

    AST* parse();

    ExternalDeclaration* parseExternalDeclaration();

    CompoundStatement* parseCompoundStatement();

    BlockItemList* parseBlockItemList();

    Statement* parseStatement();

    Expression* parseExpression();

    ConditionalExpression* parseConditionalExpression();

    BinaryExpression* parseBinaryExpression();

    UnaryExpression* parseUnaryExpression();

    PostFixExpression* parsePostFixExpression();

    PrimaryExpression* parsePrimaryExpression();
};
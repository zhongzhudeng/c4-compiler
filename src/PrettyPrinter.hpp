#include "Parser.hpp"

class PrettyPrinter{
    public:
    PrettyPrinter(AST* ast);

    void printAST();

    private:
    AST* ast;

    void printExternalDeclaration(ExternalDeclaration* exDecl);

    void printCompoundStatement(CompoundStatement* compSt);

    void printStatement(Statement* st);

    void printExpression(Expression* expr);

    void printConditionalExpression(ConditionalExpression* condExpr);

    void printBinaryExpression(BinaryExpression* biExpr);

    void printUnaryExpression(UnaryExpression* unExpr);

    void printPostFixExpression(PostFixExpression* postFixExpr);

    void printPrimaryExpression(PrimaryExpression* primExpr);

    void printSpecDecl(SpecDecl* spDecl);

    void printDeclarator(Declarator* decl);

    void printNonFunDeclarator(Declarator* decl);
};

#include "parser.h"
#include "PrettyPrinter.hpp"

PrettyPrinter::PrettyPrinter(AST *ast) {
    this->ast = ast;
}

void PrettyPrinter::printAST() {

}

void PrettyPrinter::printExternalDeclaration(ExternalDeclaration* exDecl) {

}

void PrettyPrinter::printCompoundStatement(CompoundStatement* compSt) {

}

void PrettyPrinter::printStatement(Statement* st) {

}

void PrettyPrinter::printExpression(Expression* expr) {

}

void PrettyPrinter::printConditionalExpression(ConditionalExpression* condExpr) {

}

void PrettyPrinter::printBinaryExpression(BinaryExpression* biExpr) {

}

void PrettyPrinter::printUnaryExpression(UnaryExpression* unExpr) {

}

void PrettyPrinter::printPostFixExpression(PostFixExpression* postFixExpr) {
    
}

void PrettyPrinter::printPrimaryExpression(PrimaryExpression* primExpr) {
    if (primExpr->token != nullptr) {
        std::cout << primExpr->token->Text;
    } else {
        std::cout << "(";
        printExpression(primExpr->expr);
        std::cout << ")";
    }
}

void PrettyPrinter::printSpecDecl(SpecDecl* spDecl) {

}

void PrettyPrinter::printDeclarator(Declarator* decl) {

}

void PrettyPrinter::printNonFunDeclarator(Declarator* decl) {

}

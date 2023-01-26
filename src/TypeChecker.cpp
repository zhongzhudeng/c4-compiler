#include "TypeChecker.hpp"
#include "Scope.hpp"
#include "TypeVisitor.hpp"
#include "SyntaxStmt.hpp"

class stmtTypeChecker : public StmtVisitor {
    void CheckStmt(const Stmt& stmt) {

    }
    void CheckExp(const Expr& expr) {

    }
    void CheckCondExpr(const Expr& expr) {
        CheckExp(expr);
    }

    void Visit(IdentifierStmt& stmt) override {

    };
    void Visit(CompStmt& stmt) override {

    };
    void Visit(ExprStmt& stmt) override {

    };
    void Visit(IfStmt& stmt) override {
        CheckCondExpr( stmt.getCond() );
        CheckStmt( stmt.getIf() );
    };
    void Visit(IfElseStmt& stmt) override {
        CheckCondExpr( stmt.getCond() );
        CheckStmt( stmt.getIf() );
        CheckStmt( stmt.getElse() );
    };
    void Visit(WhileStmt& stmt) override {
        CheckCondExpr( stmt.getCond() );
        CheckStmt( stmt.getDo() );
    };
    void Visit(GoToStmt& stmt) override {

    };
    void Visit(ContinueStmt& stmt) override {

    };
    void Visit(BreakStmt& stmt) override {

    };
};

// returns zero for succesful type check, returns non-zero value otherwise
int checkType(Program &prog) {
    return 0;
}

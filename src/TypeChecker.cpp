#include "TypeChecker.hpp"
#include "Scope.hpp"
#include "TypeVisitor.hpp"
#include "SyntaxStmt.hpp"

#include <map>

//namespace {    
// What is the type of Function Definitions
// The function table is a multimap, mapping function names to overloaded definitions.
//using FuncTable = std::multimap<std::string, const FuncDef*>;
//}

class exprTypeChecekr : public ExprVisitor {
  public:
    void* Visit(IdentifierExpr& exp) override {

    }
    void* Visit(ConstantExpr& exp) override {

    }
    void* Visit(StringLiteralExpr& exp) override {
        
    }
    void* Visit(ParenExpr& exp) override {
        
    }
    void* Visit(AssignmentExpr& exp) override {
        
    }
    void* Visit(ConditionalExpr& exp) override {
        
    }
    void* Visit(BinaryExpr& exp) override {
        
    }
    void* Visit(UnaryExpr& exp) override {
        
    }
    void* Visit(SizeofExpr& exp) override {
        
    }
    void* Visit(CastExpr& exp) override {
        
    }
    void* Visit(ArraySubscriptExpr& exp) override {
        
    }
    void* Visit(FunctionCallExpr& exp) override {
        
    }
    void* Visit(MemberExpr& exp) override {
        
    }
};

class stmtTypeChecker : public StmtVisitor {
    public:
    //stmtTypeChecker(Scope* scope, const FuncTable& funcTable, const FuncDef& enclosingFunction)
    //    : m_scope(scope), m_funcTable(funcTable), m_enclosingFunction(enclosingFunction){}
    void CheckStmt(const Stmt& stmt) {

    }
    void CheckExp(const Expr& expr) {

    }
    void CheckCondExpr(const Expr& expr) {
        CheckExp(expr);
    }

    void Visit(IdentifierStmt& stmt) override {
        // TODO: Look up id in scope
        
        CheckStmt(stmt.getStmt());
    };
    void Visit(CompStmt& stmt) override {

    };
    void Visit(ExprStmt& stmt) override {
        if (stmt.getHasExpr()) {
            CheckExp(stmt.getExpr());
        }
    };
    void Visit(IfStmt& stmt) override {
        CheckCondExpr(stmt.getCond());
        CheckStmt(stmt.getIf());
    };
    void Visit(IfElseStmt& stmt) override {
        CheckCondExpr(stmt.getCond());
        CheckStmt(stmt.getIf());
        CheckStmt(stmt.getElse());
    };
    void Visit(WhileStmt& stmt) override {
        CheckCondExpr(stmt.getCond());
        CheckStmt(stmt.getDo());
    };
    void Visit(GoToStmt& stmt) override {

    };
    void Visit(ContinueStmt& stmt) override {

    };
    void Visit(BreakStmt& stmt) override {

    };
    private:
        Scope* m_scope;
        //const FuncTable& m_funcTable;
        //const FuncDef&   m_enclosingFunction;
};

// returns zero for succesful type check, returns non-zero value otherwise
int checkType(Program &prog) {
    return 0;
}

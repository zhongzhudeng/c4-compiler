#pragma once

#include "Syntax.hpp"
#include "SyntaxExpr.hpp"

class ExprVisitor
{
  public:    
    virtual void* Visit(IdentifierExpr& exp) = 0;
    virtual void* Visit(ConstantExpr& exp)  = 0;
    virtual void* Visit(StringLiteralExpr& exp)  = 0;
    virtual void* Visit(ParenExpr& exp) = 0;
    virtual void* Visit(AssignmentExpr& exp)  = 0;
    virtual void* Visit(ConditionalExpr& exp)  = 0;
    virtual void* Visit(BinaryExpr& exp) = 0;
    virtual void* Visit(UnaryExpr& exp)  = 0;
    virtual void* Visit(SizeofExpr& exp)  = 0;
    virtual void* Visit(CastExpr& exp) = 0;
    virtual void* Visit(ArraySubscriptExpr& exp)  = 0;
    virtual void* Visit(FunctionCallExpr& exp)  = 0;
    virtual void* Visit(MemberExpr& exp) = 0;
};


/// Statement visitor base class.
class StmtVisitor
{
  public:
    virtual void Visit(IdentifierStmt& stmt) = 0;
    virtual void Visit(CompStmt& stmt) = 0;
    virtual void Visit(ExprStmt& stmt) = 0;
    virtual void Visit(IfStmt& stmt) = 0;
    virtual void Visit(IfElseStmt& stmt) = 0;
    virtual void Visit(WhileStmt& stmt) = 0;
    virtual void Visit(GoToStmt& stmt) = 0;
    virtual void Visit(ContinueStmt& stmt) = 0;
    virtual void Visit(BreakStmt& stmt) = 0;
};
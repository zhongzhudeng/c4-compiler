#pragma once

#include "Syntax.hpp"

class ExprVisitor
{
  public:
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
#pragma once
#include "Diagnostic.hpp"
#include <memory>

class Expr;
class IdentifierExpr;
class ConstantExpr;
class StringLiteralExpr;
class ParenExpr;
class ConditionalExpr;
class BinaryExpr;
class UnaryExpr;
class SizeofExpr;
class SizeofTypeExpr;
class CastExpr;
class ArraySubscriptExpr;
class FunctionCallExpr;
class MemberExpr;

class Decl;
enum DeclKind { ANY, ABSTRACT, CONCRETE };

class DeclSpec;
class VoidSpec;
class CharSpec;
class IntSpec;
class StructSpec;

class Declarator;
class PrimitiveDeclarator;
class PointerDeclarator;
class FunctionDeclarator;

class Stmt;
class IdentifierStmt;
class CompStmt;
class ExprStmt;
class IfStmt;
class IfElseStmt;
class WhileStmt;
class GoToStmt;
class ContinueStmt;
class BreakStmt;
class ReturnStmt;

class ExternDecl;
class FuncDef;

using ExprPtr = std::unique_ptr<Expr>;
using DeclPtr = std::unique_ptr<Decl>;
using StmtPtr = std::unique_ptr<Stmt>;
using ExternDeclPtr = std::unique_ptr<ExternDecl>;

using DeclSpecPtr = std::unique_ptr<DeclSpec>;

using DeclaratorPtr = std::unique_ptr<Declarator>;

using CompStmtPtr = std::unique_ptr<CompStmt>;
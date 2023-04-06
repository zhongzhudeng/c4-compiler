#pragma once
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
using ExprPtr = std::unique_ptr<Expr>;
using IdentifierExprPtr = std::unique_ptr<IdentifierExpr>;
using ArraySubscriptExprPtr = std::unique_ptr<ArraySubscriptExpr>;

class DeclSpec;
class VoidSpec;
class CharSpec;
class IntSpec;
class StructSpec;
using DeclSpecPtr = std::unique_ptr<DeclSpec>;
using VoidSpecPtr = std::unique_ptr<VoidSpec>;
using CharSpecPtr = std::unique_ptr<CharSpec>;
using StructSpecPtr = std::unique_ptr<StructSpec>;

class Declarator;
class NamedDeclarator;
class ValuedDeclarator;
class VariableDeclarator;
class FunctionVariableDeclarator;
class FunctionDeclarator;
class AbstractDeclarator;
class AbstractVariableDeclarator;
class AbstractFunctionVariableDeclarator;
using DeclaratorPtr = std::unique_ptr<Declarator>;
using NamedDeclaratorPtr = std::unique_ptr<NamedDeclarator>;
using ValuedDeclaratorPtr = std::unique_ptr<ValuedDeclarator>;
using VariableDeclaratorPtr = std::unique_ptr<VariableDeclarator>;
using FunctionVariableDeclaratorPtr =
    std::unique_ptr<FunctionVariableDeclarator>;
using FunctionDeclaratorPtr = std::unique_ptr<FunctionDeclarator>;
using AbstractDeclaratorPtr = std::unique_ptr<AbstractDeclarator>;
using AbstractVariableDeclaratorPtr =
    std::unique_ptr<AbstractVariableDeclarator>;
using AbstractFunctionVariableDeclaratorPtr =
    std::unique_ptr<AbstractFunctionVariableDeclarator>;

class TranslationUnit;
class Decl;
class DeclaratorDecl;
class NamedDeclaratorDecl;
class LabelDecl;
class FunctionDefinition;
class StructDef;
using TranslationUnitPtr = std::unique_ptr<TranslationUnit>;
using DeclPtr = std::unique_ptr<Decl>;
using DeclaratorDeclPtr = std::unique_ptr<DeclaratorDecl>;
using NamedDeclaratorDeclPtr = std::unique_ptr<NamedDeclaratorDecl>;
using LabelDeclPtr = std::unique_ptr<LabelDecl>;
using FuncDefPtr = std::unique_ptr<FunctionDefinition>;
using StructDefPtr = std::unique_ptr<StructDef>;

class Stmt;

class LabeledStmt;
class CompStmt;
class ExprStmt;
class IfElseStmt;
class WhileStmt;
class GoToStmt;
class ContinueStmt;
class BreakStmt;
class ReturnStmt;

using StmtPtr = std::unique_ptr<Stmt>;
using LabeledStmtPtr = std::unique_ptr<LabeledStmt>;
using CompStmtPtr = std::unique_ptr<CompStmt>;
using GoToStmtPtr = std::unique_ptr<GoToStmt>;
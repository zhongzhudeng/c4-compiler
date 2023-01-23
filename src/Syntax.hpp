#pragma once
#include "Diagnostic.hpp"
#include <memory>
#include <vector>

class AST;
class TranslationUnit;

class ExternalDecl;

class Expr;
class IdentifierExpr;
class ConstantExpr;
class StringLiteralExpr;
class ParenExpr;
class AssignmentExpr;
class ConditionalExpr;
class BinaryExpr;
class UnaryExpr;
class SizeofExpr;
class SizeofTypeExpr;
class CastExpr;
class ArraySubscriptExpr;
class FunctionCallExpr;
class MemberExpr;

class Declaration;
class StructDeclaration;
class ParameterDeclaration;

enum class DeclKind { ANY, ABSTRACT, CONCRETE };
class Declarator;
using StructDeclarator = Declarator;
class PrimitiveDeclarator;
class PointerDeclarator;
class FunctionDeclarator;

class AbstractDeclarator;
class AbstractPointerDeclarator;
class AbstractFunctionDeclarator;

class TypeSpecifier;
using DeclarationSpecifier = TypeSpecifier;
class VoidSpecifier;
class CharSpecifier;
class IntSpecifier;
class StructSpecifier;

using SpecifierQualifier = TypeSpecifier;

class Pointer;
class DirectDeclarator;

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
using StmtPtr = std::unique_ptr<Stmt>;

using ASTPtr = std::unique_ptr<AST>;

using ExprPtr = std::unique_ptr<Expr>;

using DeclarationPtr = std::unique_ptr<Declaration>;
using StructDeclarationPtr = std::unique_ptr<StructDeclaration>;
using ParameterDeclarationPtr = std::unique_ptr<ParameterDeclaration>;

using TypeSpecifierPtr = std::unique_ptr<TypeSpecifier>;
using DeclarationSpecifierPtr = std::unique_ptr<DeclarationSpecifier>;
using StructSpecifierPtr = std::unique_ptr<StructSpecifier>;
using StructDeclarationPtr = std::unique_ptr<StructDeclaration>;
using SpecifierQualifierPtr = std::unique_ptr<SpecifierQualifier>;

using SpecifierQualifierPtr = std::unique_ptr<SpecifierQualifier>;

using DeclaratorPtr = std::unique_ptr<Declarator>;
using StructDeclaratorPtr = std::unique_ptr<StructDeclarator>;

using PrimitiveDeclaratorPtr = std::unique_ptr<PrimitiveDeclarator>;
using PointerDeclaratorPtr = std::unique_ptr<PointerDeclarator>;
using FunctionDeclaratorPtr = std::unique_ptr<FunctionDeclarator>;

using PointerPtr = std::unique_ptr<Pointer>;
using DirectDeclaratorPtr = std::unique_ptr<DirectDeclarator>;

using TranslationUnitPtr = std::unique_ptr<TranslationUnit>;
using ExternalDeclPtr = std::unique_ptr<ExternalDecl>;

class ASTPrinter;
class AST {
  Locatable loc;

public:
  AST(Locatable loc);
  virtual ~AST() = 0;

  virtual void accept(ASTPrinter &visitor) = 0;
  const Locatable &getLoc();
};

class Expr : public AST {
public:
  Expr(Locatable loc);
  virtual void accept(ASTPrinter &visitor) override;
  virtual ~Expr(){};
};

class Stmt : public AST {
public:
  Stmt(Locatable loc);
  virtual void accept(ASTPrinter &visitor) override;
  virtual bool isIf() const{
    return false;
  }
  virtual bool isComp() const{
    return false;
  }
  virtual ~Stmt(){};
};
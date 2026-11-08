#include "CodeGen.hpp"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

llvm::Type *opaquePointer(llvm::LLVMContext &context) {
  return llvm::PointerType::get(context, 0);
}

} // namespace

CodeGen::CodeGen(const AST &ast, const SemanticModel &model, TypeLayout &layout,
                 llvm::LLVMContext &context, llvm::Module &module,
                 Diagnostics &diagnostics)
    : ast_(ast), model_(model), layout_(layout), context_(context),
      module_(module), diagnostics_(diagnostics), builder_(context),
      allocaBuilder_(context) {}

bool CodeGen::isPointerLike(const Type *type) {
  return Type::isPointer(type) || Type::isArray(type) || Type::isFunction(type);
}

const Type *CodeGen::elementType(const Type *type) {
  if (Type::isPointer(type) || Type::isArray(type))
    return type->element;
  return type;
}

void CodeGen::emit() {
  createExternalEntities();

  for (DeclId id : ast_.translationUnit().decls) {
    if (auto *function = std::get_if<FunctionDecl>(&ast_.decl(id).node)) {
      if (function->body.valid())
        emitFunction(id, *function);
    }
  }

  std::string error;
  llvm::raw_string_ostream stream(error);
  if (llvm::verifyModule(module_, &stream)) {
    stream.flush();
    throw std::runtime_error("generated invalid LLVM IR: " + error);
  }
}

void CodeGen::createExternalEntities() {
  for (size_t i = 0; i < model_.entityCount(); ++i) {
    EntityId id{static_cast<uint32_t>(i)};
    const Entity &entity = model_.entity(id);
    if (entity.linkage != Linkage::External)
      continue;
    if (entity.kind == EntityKind::Function)
      functionFor(id);
    else
      globalFor(id);
  }
}

llvm::Function *CodeGen::functionFor(EntityId id) {
  auto it = functions_.find(id.index);
  if (it != functions_.end())
    return it->second;

  const Entity &entity = model_.entity(id);
  assert(entity.name && "external function without a name");
  auto *functionType =
      llvm::cast<llvm::FunctionType>(layout_.llvmType(entity.type));
  llvm::Function *function = llvm::Function::Create(
      functionType, llvm::GlobalValue::ExternalLinkage, *entity.name,
      &module_);
  functions_[id.index] = function;
  return function;
}

llvm::GlobalVariable *CodeGen::globalFor(EntityId id) {
  auto it = globals_.find(id.index);
  if (it != globals_.end())
    return it->second;

  const Entity &entity = model_.entity(id);
  assert(entity.name && "external object without a name");
  llvm::Type *llvmType = layout_.llvmType(entity.type);
  auto *global = new llvm::GlobalVariable(
      module_, llvmType, /*isConstant=*/false,
      llvm::GlobalValue::ExternalLinkage, llvm::Constant::getNullValue(llvmType),
      *entity.name);
  globals_[id.index] = global;
  return global;
}

llvm::Value *CodeGen::addressOfEntity(EntityId id) {
  const Entity &entity = model_.entity(id);
  assert(entity.type && "declaration without a type");
  if (entity.kind == EntityKind::Function)
    return functionFor(id);
  if (entity.linkage == Linkage::External)
    return globalFor(id);

  auto it = locals_.find(id.index);
  if (it != locals_.end())
    return it->second;

  // A local object is always declared before its first use, so this is only a
  // defensive fallback for declarations that were not materialized.
  assert(entity.name && "local declaration without a name");
  return globalFor(id);
}

llvm::AllocaInst *CodeGen::createAlloca(const Type *type, Symbol name) {
  llvm::Type *llvmType = layout_.llvmType(type);
  // Locals may be declared after the entry block already has a terminator
  // (for example inside an if or while body), so always insert new allocas
  // before the first instruction of the entry block rather than appending.
  llvm::BasicBlock *entry = &function_->getEntryBlock();
  allocaBuilder_.SetInsertPoint(entry, entry->getFirstInsertionPt());
  return allocaBuilder_.CreateAlloca(llvmType, nullptr,
                                     name ? *name : "tmp");
}

void CodeGen::emitFunction(DeclId id, const FunctionDecl &decl) {
  EntityId entityId = model_.decl(id).entity;
  const Entity &entity = model_.entity(entityId);
  llvm::Function *function = functionFor(entityId);
  if (!function->empty())
    return;

  if (entity.parameters.size() != function->arg_size())
    throw std::runtime_error("function declaration/definition parameter mismatch");

  function_ = function;
  returnType_ = entity.type->result;
  locals_.clear();
  labels_.clear();
  loops_.clear();

  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context_, "entry", function);
  builder_.SetInsertPoint(entry);
  allocaBuilder_.SetInsertPoint(entry, entry->begin());

  unsigned index = 0;
  for (llvm::Argument &argument : function->args()) {
    DeclId paramId = entity.parameters[index];
    EntityId paramEntityId = model_.decl(paramId).entity;
    const Entity &param = model_.entity(paramEntityId);
    llvm::AllocaInst *slot = createAlloca(param.type, param.name);
    allocaBuilder_.CreateStore(&argument, slot);
    locals_[paramEntityId.index] = slot;
    argument.setName(param.name ? *param.name : "arg");
    ++index;
  }

  collectLabels(decl.body);
  emitStmt(decl.body);

  if (!currentBlockTerminated()) {
    if (Type::isVoid(returnType_))
      builder_.CreateRetVoid();
    else
      builder_.CreateRet(
          llvm::Constant::getNullValue(layout_.llvmType(returnType_)));
  }

  function_ = nullptr;
  returnType_ = nullptr;
}

void CodeGen::collectLabels(StmtId id) {
  const Stmt &stmt = ast_.stmt(id);
  if (auto *compound = std::get_if<CompoundStmt>(&stmt.node)) {
    for (StmtId item : compound->items)
      collectLabels(item);
    return;
  }
  if (auto *label = std::get_if<LabelStmt>(&stmt.node)) {
    const LabelDecl &decl = ast_.label(label->label);
    labels_[label->label.index] = llvm::BasicBlock::Create(
        context_, decl.name ? *decl.name : "label", function_);
    collectLabels(label->stmt);
    return;
  }
  if (auto *ifStmt = std::get_if<IfStmt>(&stmt.node)) {
    collectLabels(ifStmt->thenStmt);
    if (ifStmt->elseStmt.valid())
      collectLabels(ifStmt->elseStmt);
    return;
  }
  if (auto *whileStmt = std::get_if<WhileStmt>(&stmt.node))
    collectLabels(whileStmt->body);
}

void CodeGen::ensureInsertionPoint() {
  llvm::BasicBlock *block = builder_.GetInsertBlock();
  assert(block && "no insertion point");
  if (block->empty() || !block->back().isTerminator())
    return;
  llvm::BasicBlock *next =
      llvm::BasicBlock::Create(context_, "dead", function_);
  builder_.SetInsertPoint(next);
}

bool CodeGen::currentBlockTerminated() const {
  llvm::BasicBlock *block = builder_.GetInsertBlock();
  return block && !block->empty() && block->back().isTerminator();
}

void CodeGen::emitCompound(StmtId id) {
  auto *compound = std::get_if<CompoundStmt>(&ast_.stmt(id).node);
  assert(compound && "expected a compound statement");
  for (StmtId item : compound->items)
    emitStmt(item);
}

void CodeGen::emitLabel(StmtId id, const LabelStmt &label) {
  (void)id;
  auto it = labels_.find(label.label.index);
  assert(it != labels_.end() && "label block was not pre-created");
  llvm::BasicBlock *block = it->second;
  if (!currentBlockTerminated())
    builder_.CreateBr(block);
  builder_.SetInsertPoint(block);
  emitStmt(label.stmt);
}

void CodeGen::emitLocalDecl(DeclId id) {
  auto *var = std::get_if<VarDecl>(&ast_.decl(id).node);
  if (!var)
    return;
  EntityId entityId = model_.decl(id).entity;
  const Entity &entity = model_.entity(entityId);
  if (entity.linkage == Linkage::External)
    return;
  if (locals_.count(entityId.index))
    return;
  locals_[entityId.index] = createAlloca(entity.type, entity.name);
}

void CodeGen::emitStmt(StmtId id) {
  const Stmt &stmt = ast_.stmt(id);

  if (auto *label = std::get_if<LabelStmt>(&stmt.node)) {
    emitLabel(id, *label);
    return;
  }

  ensureInsertionPoint();

  if (std::holds_alternative<CompoundStmt>(stmt.node)) {
    emitCompound(id);
    return;
  }

  if (auto *declStmt = std::get_if<DeclStmt>(&stmt.node)) {
    emitLocalDecl(declStmt->decl);
    return;
  }

  if (auto *exprStmt = std::get_if<ExprStmt>(&stmt.node)) {
    if (exprStmt->expr.valid())
      emitExpr(exprStmt->expr);
    return;
  }

  if (auto *ifStmt = std::get_if<IfStmt>(&stmt.node)) {
    llvm::Value *condition = toBool(ifStmt->cond);
    llvm::BasicBlock *thenBlock =
        llvm::BasicBlock::Create(context_, "if.then", function_);
    llvm::BasicBlock *elseBlock =
        ifStmt->elseStmt.valid()
            ? llvm::BasicBlock::Create(context_, "if.else", function_)
            : nullptr;
    llvm::BasicBlock *endBlock =
        llvm::BasicBlock::Create(context_, "if.end", function_);
    builder_.CreateCondBr(condition, thenBlock,
                          elseBlock ? elseBlock : endBlock);

    builder_.SetInsertPoint(thenBlock);
    emitStmt(ifStmt->thenStmt);
    if (!currentBlockTerminated())
      builder_.CreateBr(endBlock);

    if (elseBlock) {
      builder_.SetInsertPoint(elseBlock);
      emitStmt(ifStmt->elseStmt);
      if (!currentBlockTerminated())
        builder_.CreateBr(endBlock);
    }

    builder_.SetInsertPoint(endBlock);
    return;
  }

  if (auto *whileStmt = std::get_if<WhileStmt>(&stmt.node)) {
    llvm::BasicBlock *condBlock =
        llvm::BasicBlock::Create(context_, "while.cond", function_);
    llvm::BasicBlock *bodyBlock =
        llvm::BasicBlock::Create(context_, "while.body", function_);
    llvm::BasicBlock *endBlock =
        llvm::BasicBlock::Create(context_, "while.end", function_);
    builder_.CreateBr(condBlock);

    builder_.SetInsertPoint(condBlock);
    llvm::Value *condition = toBool(whileStmt->cond);
    builder_.CreateCondBr(condition, bodyBlock, endBlock);

    builder_.SetInsertPoint(bodyBlock);
    loops_.push_back(LoopContext{endBlock, condBlock});
    emitStmt(whileStmt->body);
    loops_.pop_back();
    if (!currentBlockTerminated())
      builder_.CreateBr(condBlock);

    builder_.SetInsertPoint(endBlock);
    return;
  }

  if (auto *returnStmt = std::get_if<ReturnStmt>(&stmt.node)) {
    if (returnStmt->expr.valid()) {
      llvm::Value *value = emitExpr(returnStmt->expr);
      builder_.CreateRet(coerce(value, returnType_, stmt.loc));
    } else {
      builder_.CreateRetVoid();
    }
    return;
  }

  if (std::holds_alternative<BreakStmt>(stmt.node)) {
    assert(!loops_.empty() && "break outside of a loop");
    builder_.CreateBr(loops_.back().breakBlock);
    return;
  }

  if (std::holds_alternative<ContinueStmt>(stmt.node)) {
    assert(!loops_.empty() && "continue outside of a loop");
    builder_.CreateBr(loops_.back().continueBlock);
    return;
  }

  if (std::holds_alternative<GotoStmt>(stmt.node)) {
    LabelId target = model_.gotoTarget(id);
    auto it = labels_.find(target.index);
    assert(it != labels_.end() && "unresolved goto target");
    builder_.CreateBr(it->second);
    return;
  }

  diagnostics_.error(stmt.loc,
                     "internal error: unsupported statement in code generation");
}

llvm::Value *CodeGen::emitLoad(const Type *type, llvm::Value *address) {
  return builder_.CreateLoad(layout_.llvmType(type), address);
}

llvm::Value *CodeGen::emitStringLiteral(ExprId id,
                                        const StringLiteralExpr &string) {
  (void)string;
  const ExprInfo &info = model_.expr(id);
  std::string data(info.stringBytes.begin(), info.stringBytes.end());
  llvm::Constant *initializer =
      llvm::ConstantDataArray::getString(context_, data, /*AddNull=*/true);
  llvm::ArrayType *arrayType =
      llvm::ArrayType::get(llvm::Type::getInt8Ty(context_), data.size() + 1);
  return new llvm::GlobalVariable(
      module_, arrayType, /*isConstant=*/true,
      llvm::GlobalValue::PrivateLinkage, initializer,
      ".str." + std::to_string(stringCounter_++));
}

llvm::Value *CodeGen::emitInt(ExprId id) {
  const Type *type = model_.expr(id).type;
  llvm::Value *value = emitExpr(id);
  if (Type::isChar(type))
    return builder_.CreateSExt(value, llvm::Type::getInt32Ty(context_), "promote");
  return value;
}

llvm::Value *CodeGen::indexValue(ExprId id) {
  llvm::Value *value = emitInt(id);
  return builder_.CreateSExt(value, llvm::Type::getInt64Ty(context_), "idx");
}

llvm::Value *CodeGen::emitPointerValue(ExprId id) {
  const ExprInfo &info = model_.expr(id);
  if (info.nullPointerConstant)
    return llvm::ConstantPointerNull::get(
        llvm::cast<llvm::PointerType>(opaquePointer(context_)));
  return emitExpr(id);
}

llvm::Value *CodeGen::toBool(ExprId id) {
  const Type *type = model_.expr(id).type;
  llvm::Value *value = emitExpr(id);
  if (isPointerLike(type))
    return builder_.CreateICmpNE(
        value, llvm::ConstantPointerNull::get(
                   llvm::cast<llvm::PointerType>(opaquePointer(context_))),
        "tobool");
  return builder_.CreateICmpNE(value, llvm::ConstantInt::get(value->getType(), 0),
                               "tobool");
}

llvm::Value *CodeGen::coerce(llvm::Value *value, const Type *to, SourceLoc loc) {
  if (!value)
    return nullptr;
  if (Type::isVoid(to))
    return nullptr;

  llvm::Type *target = layout_.llvmType(to);

  if (Type::isInteger(to)) {
    if (value->getType() == target)
      return value;
    if (value->getType()->isIntegerTy() && target->isIntegerTy()) {
      unsigned from = value->getType()->getIntegerBitWidth();
      unsigned dest = target->getIntegerBitWidth();
      if (from < dest)
        return builder_.CreateSExt(value, target);
      if (from > dest)
        return builder_.CreateTrunc(value, target);
    }
    diagnostics_.error(loc, "internal error: invalid integer conversion");
  }

  if (Type::isPointer(to)) {
    if (value->getType()->isPointerTy())
      return value;
    if (auto *constant = llvm::dyn_cast<llvm::ConstantInt>(value))
      if (constant->isZero())
        return llvm::ConstantPointerNull::get(
            llvm::cast<llvm::PointerType>(target));
    if (value->getType()->isIntegerTy())
      return builder_.CreateIntToPtr(value, target);
    diagnostics_.error(loc, "internal error: invalid pointer conversion");
  }

  if (Type::isStruct(to) && value->getType()->isPointerTy())
    return value;

  diagnostics_.error(loc, "internal error: unsupported conversion");
}

llvm::Value *CodeGen::emitExpr(ExprId id) {
  const ExprInfo &info = model_.expr(id);
  const Expr &expr = ast_.expr(id);

  if (info.isConstant)
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_),
                                  static_cast<uint64_t>(info.value));

  if (auto *identifier = std::get_if<IdentifierExpr>(&expr.node)) {
    (void)identifier;
    llvm::Value *address = addressOfEntity(info.entity);
    const Type *type = info.type;
    if (Type::isFunction(type) || Type::isArray(type) || Type::isStruct(type))
      return address;
    return emitLoad(type, address);
  }

  if (auto *string = std::get_if<StringLiteralExpr>(&expr.node))
    return emitStringLiteral(id, *string);

  if (auto *unary = std::get_if<UnaryExpr>(&expr.node)) {
    switch (unary->op) {
    case UnaryExpr::Op::Plus:
      return emitInt(unary->operand);
    case UnaryExpr::Op::Minus:
      return builder_.CreateNeg(emitInt(unary->operand), "neg");
    case UnaryExpr::Op::Not: {
      llvm::Value *value = toBool(unary->operand);
      llvm::Value *negated =
          builder_.CreateXor(value, llvm::ConstantInt::getTrue(context_));
      return builder_.CreateZExt(negated, llvm::Type::getInt32Ty(context_),
                                 "not");
    }
    case UnaryExpr::Op::Deref: {
      llvm::Value *address = emitExpr(unary->operand);
      const Type *type = info.type;
      if (Type::isFunction(type) || Type::isArray(type) || Type::isStruct(type))
        return address;
      return emitLoad(type, address);
    }
    case UnaryExpr::Op::Addr:
      return emitAddress(unary->operand);
    }
  }

  if (auto *binary = std::get_if<BinaryExpr>(&expr.node))
    return emitBinary(id, *binary);

  if (auto *assign = std::get_if<AssignExpr>(&expr.node)) {
    llvm::Value *address = emitAddress(assign->lhs);
    const Type *type = model_.expr(assign->lhs).type;
    llvm::Value *value = coerce(emitExpr(assign->rhs), type, expr.loc);
    builder_.CreateStore(value, address);
    return value;
  }

  if (auto *conditional = std::get_if<ConditionalExpr>(&expr.node))
    return emitConditional(id, *conditional);

  if (auto *call = std::get_if<CallExpr>(&expr.node))
    return emitCall(id, *call);

  if (std::holds_alternative<SubscriptExpr>(expr.node) ||
      std::holds_alternative<MemberExpr>(expr.node)) {
    llvm::Value *address = emitAddress(id);
    const Type *type = info.type;
    if (Type::isFunction(type) || Type::isArray(type) || Type::isStruct(type))
      return address;
    return emitLoad(type, address);
  }

  if (std::holds_alternative<SizeofExpr>(expr.node))
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_),
                                  static_cast<uint64_t>(info.value));

  diagnostics_.error(
      expr.loc, "internal error: unsupported expression in code generation");
}

llvm::Value *CodeGen::emitAddress(ExprId id) {
  const ExprInfo &info = model_.expr(id);
  const Expr &expr = ast_.expr(id);

  if (std::holds_alternative<IdentifierExpr>(expr.node))
    return addressOfEntity(info.entity);

  if (auto *string = std::get_if<StringLiteralExpr>(&expr.node))
    return emitStringLiteral(id, *string);

  if (auto *unary = std::get_if<UnaryExpr>(&expr.node)) {
    if (unary->op == UnaryExpr::Op::Deref)
      return emitExpr(unary->operand);
  }

  if (auto *subscript = std::get_if<SubscriptExpr>(&expr.node)) {
    const ExprInfo &baseInfo = model_.expr(subscript->base);
    const ExprInfo &indexInfo = model_.expr(subscript->index);
    llvm::Value *base = nullptr;
    llvm::Value *index = nullptr;
    const Type *element = nullptr;
    if (isPointerLike(baseInfo.type)) {
      base = emitExpr(subscript->base);
      index = indexValue(subscript->index);
      element = elementType(baseInfo.type);
    } else {
      base = emitExpr(subscript->index);
      index = indexValue(subscript->base);
      element = elementType(indexInfo.type);
    }
    llvm::Value *indices[] = {index};
    return builder_.CreateGEP(layout_.llvmType(element), base, indices,
                              "index");
  }

  if (auto *member = std::get_if<MemberExpr>(&expr.node)) {
    llvm::Value *base =
        member->arrow ? emitExpr(member->base) : emitAddress(member->base);
    assert(info.memberRecord && "member access without a record");
    llvm::StructType *structType = layout_.llvmStruct(info.memberRecord);
    return builder_.CreateStructGEP(structType, base, info.memberIndex,
                                    "member");
  }

  diagnostics_.error(expr.loc,
                     "internal error: expression has no address");
}

llvm::Value *CodeGen::emitBinary(ExprId id, const BinaryExpr &binary) {
  const ExprInfo &leftInfo = model_.expr(binary.lhs);
  const ExprInfo &rightInfo = model_.expr(binary.rhs);
  const Type *leftType = leftInfo.type;
  const Type *rightType = rightInfo.type;

  switch (binary.op) {
  case BinaryExpr::Op::Mul:
    return builder_.CreateMul(emitInt(binary.lhs), emitInt(binary.rhs), "mul");

  case BinaryExpr::Op::Add: {
    if (Type::isInteger(leftType) && Type::isInteger(rightType))
      return builder_.CreateAdd(emitInt(binary.lhs), emitInt(binary.rhs),
                                "add");
    if (isPointerLike(leftType) && Type::isInteger(rightType)) {
      llvm::Value *base = emitExpr(binary.lhs);
      llvm::Value *index = indexValue(binary.rhs);
      llvm::Value *indices[] = {index};
      return builder_.CreateGEP(
          layout_.llvmType(elementType(leftType)), base, indices, "add.ptr");
    }
    llvm::Value *base = emitExpr(binary.rhs);
    llvm::Value *index = indexValue(binary.lhs);
    llvm::Value *indices[] = {index};
    return builder_.CreateGEP(layout_.llvmType(elementType(rightType)), base,
                              indices, "add.ptr");
  }

  case BinaryExpr::Op::Sub: {
    if (Type::isInteger(leftType) && Type::isInteger(rightType))
      return builder_.CreateSub(emitInt(binary.lhs), emitInt(binary.rhs),
                                "sub");
    if (isPointerLike(leftType) && Type::isInteger(rightType)) {
      llvm::Value *base = emitExpr(binary.lhs);
      llvm::Value *index = builder_.CreateNeg(indexValue(binary.rhs), "neg.idx");
      llvm::Value *indices[] = {index};
      return builder_.CreateGEP(
          layout_.llvmType(elementType(leftType)), base, indices, "sub.ptr");
    }
    llvm::Value *left = builder_.CreatePtrToInt(
        emitExpr(binary.lhs), llvm::Type::getInt64Ty(context_), "left.int");
    llvm::Value *right = builder_.CreatePtrToInt(
        emitExpr(binary.rhs), llvm::Type::getInt64Ty(context_), "right.int");
    llvm::Value *difference =
        builder_.CreateSub(left, right, "ptr.diff");
    uint64_t size = layout_.sizeOf(elementType(leftType));
    if (size > 1)
      difference = builder_.CreateSDiv(
          difference, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_),
                                             size),
          "ptr.diff.scaled");
    return builder_.CreateTrunc(difference,
                                llvm::Type::getInt32Ty(context_), "ptr.diff.int");
  }

  case BinaryExpr::Op::Less: {
    llvm::Value *result = nullptr;
    if (Type::isInteger(leftType) && Type::isInteger(rightType))
      result = builder_.CreateICmpSLT(emitInt(binary.lhs), emitInt(binary.rhs),
                                      "less");
    else
      result = builder_.CreateICmpULT(emitPointerValue(binary.lhs),
                                      emitPointerValue(binary.rhs), "less");
    return builder_.CreateZExt(result, llvm::Type::getInt32Ty(context_),
                               "less.int");
  }

  case BinaryExpr::Op::Equal:
  case BinaryExpr::Op::NotEqual: {
    bool equal = binary.op == BinaryExpr::Op::Equal;
    llvm::Value *left = nullptr;
    llvm::Value *right = nullptr;
    if (Type::isInteger(leftType) && Type::isInteger(rightType)) {
      left = emitInt(binary.lhs);
      right = emitInt(binary.rhs);
    } else {
      left = emitPointerValue(binary.lhs);
      right = emitPointerValue(binary.rhs);
    }
    llvm::Value *result = equal
                              ? builder_.CreateICmpEQ(left, right, "equal")
                              : builder_.CreateICmpNE(left, right, "notequal");
    return builder_.CreateZExt(result, llvm::Type::getInt32Ty(context_),
                               "equal.int");
  }

  case BinaryExpr::Op::LogicalAnd:
    return emitLogical(id, binary, /*isAnd=*/true);
  case BinaryExpr::Op::LogicalOr:
    return emitLogical(id, binary, /*isAnd=*/false);
  }

  diagnostics_.error(ast_.expr(id).loc,
                     "internal error: unsupported binary operator");
}

llvm::Value *CodeGen::emitLogical(ExprId id, const BinaryExpr &binary,
                                  bool isAnd) {
  (void)id;
  llvm::Value *left = toBool(binary.lhs);
  llvm::BasicBlock *leftBlock = builder_.GetInsertBlock();

  llvm::BasicBlock *rightBlock =
      llvm::BasicBlock::Create(context_, "logic.rhs", function_);
  llvm::BasicBlock *endBlock =
      llvm::BasicBlock::Create(context_, "logic.end", function_);

  if (isAnd)
    builder_.CreateCondBr(left, rightBlock, endBlock);
  else
    builder_.CreateCondBr(left, endBlock, rightBlock);

  builder_.SetInsertPoint(rightBlock);
  llvm::Value *right = toBool(binary.rhs);
  llvm::BasicBlock *rightEnd = builder_.GetInsertBlock();
  builder_.CreateBr(endBlock);

  builder_.SetInsertPoint(endBlock);
  llvm::PHINode *phi = builder_.CreatePHI(
      llvm::Type::getInt1Ty(context_), 2, "logic");
  phi->addIncoming(isAnd ? llvm::ConstantInt::getFalse(context_)
                         : llvm::ConstantInt::getTrue(context_),
                   leftBlock);
  phi->addIncoming(right, rightEnd);
  return builder_.CreateZExt(phi, llvm::Type::getInt32Ty(context_), "logic.ext");
}

llvm::Value *CodeGen::emitConditional(ExprId id,
                                      const ConditionalExpr &conditional) {
  const Type *type = model_.expr(id).type;
  SourceLoc loc = ast_.expr(id).loc;
  llvm::Value *condition = toBool(conditional.cond);
  llvm::BasicBlock *thenBlock =
      llvm::BasicBlock::Create(context_, "cond.then", function_);
  llvm::BasicBlock *elseBlock =
      llvm::BasicBlock::Create(context_, "cond.else", function_);
  llvm::BasicBlock *endBlock =
      llvm::BasicBlock::Create(context_, "cond.end", function_);
  builder_.CreateCondBr(condition, thenBlock, elseBlock);

  builder_.SetInsertPoint(thenBlock);
  llvm::Value *thenValue =
      coerce(emitExpr(conditional.thenExpr), type, loc);
  llvm::BasicBlock *thenEnd = builder_.GetInsertBlock();
  builder_.CreateBr(endBlock);

  builder_.SetInsertPoint(elseBlock);
  llvm::Value *elseValue =
      coerce(emitExpr(conditional.elseExpr), type, loc);
  llvm::BasicBlock *elseEnd = builder_.GetInsertBlock();
  builder_.CreateBr(endBlock);

  builder_.SetInsertPoint(endBlock);

  if (Type::isVoid(type))
    return nullptr;

  llvm::PHINode *phi =
      builder_.CreatePHI(layout_.llvmType(type), 2, "cond");
  phi->addIncoming(thenValue, thenEnd);
  phi->addIncoming(elseValue, elseEnd);
  return phi;
}

llvm::Value *CodeGen::emitCall(ExprId id, const CallExpr &call) {
  const Type *calleeType = model_.expr(call.callee).type;
  const Type *functionType = nullptr;
  if (Type::isFunction(calleeType))
    functionType = calleeType;
  else if (Type::isPointer(calleeType) && Type::isFunction(calleeType->element))
    functionType = calleeType->element;

  if (!functionType)
    diagnostics_.error(ast_.expr(id).loc,
                       "internal error: call to a non-function value");

  llvm::Value *callee = emitExpr(call.callee);
  std::vector<llvm::Value *> arguments;
  arguments.reserve(call.args.size());
  for (size_t i = 0; i < call.args.size(); ++i) {
    llvm::Value *value = emitExpr(call.args[i]);
    arguments.push_back(
        coerce(value, functionType->params[i], ast_.expr(call.args[i]).loc));
  }

  return builder_.CreateCall(
      llvm::cast<llvm::FunctionType>(layout_.llvmType(functionType)), callee,
      arguments);
}

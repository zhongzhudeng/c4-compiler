#include "CodeGen.hpp"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"

#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

llvm::PointerType *opaquePointer(llvm::LLVMContext &context) {
  return llvm::PointerType::get(context, 0);
}

/// Internal code-generation failures are compiler bugs, not source-language
/// diagnostics.  They become "c4: internal error: ..." in main and exit 2.
[[noreturn]] void fail(const std::string &message) {
  throw std::runtime_error("codegen: " + message);
}

[[noreturn]] void failConversion(const Type *from, const Type *to) {
  fail("cannot convert '" + TypeContext::toString(from) + "' to '" +
       TypeContext::toString(to) + "'");
}

std::string entityName(const Entity &entity) {
  return entity.name ? *entity.name : std::string("<anonymous>");
}

} // namespace

CodeGen::CodeGen(const AST &ast, const SemanticModel &model, TypeContext &types,
                 TypeLayout &layout, llvm::LLVMContext &context,
                 llvm::Module &module)
    : ast_(ast), model_(model), types_(types), layout_(layout),
      context_(context), module_(module) {}

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
  if (entity.kind != EntityKind::Function)
    fail("entity '" + entityName(entity) + "' is not a function");
  if (!entity.name)
    fail("external function without a name");
  if (!entity.type || !Type::isFunction(entity.type))
    fail("function entity '" + entityName(entity) + "' has non-function type");

  auto *functionType =
      llvm::cast<llvm::FunctionType>(layout_.llvmType(entity.type));
  llvm::Function *function = llvm::Function::Create(
      functionType, llvm::GlobalValue::ExternalLinkage, *entity.name, &module_);
  functions_[id.index] = function;
  return function;
}

llvm::GlobalVariable *CodeGen::globalFor(EntityId id) {
  auto it = globals_.find(id.index);
  if (it != globals_.end())
    return it->second;

  const Entity &entity = model_.entity(id);
  if (entity.kind != EntityKind::Object)
    fail("entity '" + entityName(entity) + "' is not an object");
  if (!entity.name)
    fail("external object without a name");
  if (!entity.type || !Type::isObject(entity.type))
    fail("object entity '" + entityName(entity) + "' has non-object type");

  llvm::Type *llvmType = layout_.llvmType(entity.type);
  auto *global = new llvm::GlobalVariable(
      module_, llvmType, /*isConstant=*/false,
      llvm::GlobalValue::ExternalLinkage,
      llvm::Constant::getNullValue(llvmType), *entity.name);
  globals_[id.index] = global;
  return global;
}

llvm::GlobalVariable *CodeGen::stringLiteral(const ExprInfo &info) {
  std::string data(info.stringBytes.begin(), info.stringBytes.end());
  llvm::Constant *initializer =
      llvm::ConstantDataArray::getString(context_, data, /*AddNull=*/true);
  llvm::ArrayType *arrayType =
      llvm::ArrayType::get(llvm::Type::getInt8Ty(context_), data.size() + 1);
  return new llvm::GlobalVariable(module_, arrayType, /*isConstant=*/true,
                                  llvm::GlobalValue::PrivateLinkage,
                                  initializer,
                                  ".str." + std::to_string(stringCounter_++));
}

/// Lowers one function definition.  Every piece of function-specific state
/// (current function, return type, builders, locals, labels and loops) lives
/// here, so two consecutive definitions cannot share or leak state.
class CodeGen::FunctionEmitter {
public:
  FunctionEmitter(CodeGen &codegen, DeclId declId)
      : codegen_(codegen), ast_(codegen.ast_), model_(codegen.model_),
        types_(codegen.types_), layout_(codegen.layout_),
        context_(codegen.context_), declId_(declId), builder_(codegen.context_),
        allocaBuilder_(codegen.context_) {
    entityId_ = model_.decl(declId_).entity;
  }

  void emit();

private:
  /// An addressable object (or string-literal array) and its C4 type.
  struct Place {
    llvm::Value *address = nullptr;
    const Type *type = nullptr;
  };

  /// A computed scalar value (integer or pointer) and its C4 type.  Void,
  /// struct, array and function values are deliberately not representable.
  struct RValue {
    llvm::Value *value = nullptr;
    const Type *type = nullptr;
  };

  /// A callable entity.  callee is either an llvm::Function (direct call) or
  /// a pointer value (indirect call); type is the C4 function type.
  struct FunctionDesignator {
    llvm::Value *callee = nullptr;
    const Type *type = nullptr;
  };

  /// Evaluation with no resulting value (void call or void conditional).
  struct VoidResult {};

  using ExprResult =
      std::variant<Place, RValue, FunctionDesignator, VoidResult>;

  struct LoopContext {
    llvm::BasicBlock *breakBlock = nullptr;
    llvm::BasicBlock *continueBlock = nullptr;
  };

  CodeGen &codegen_;
  const AST &ast_;
  const SemanticModel &model_;
  TypeContext &types_;
  TypeLayout &layout_;
  llvm::LLVMContext &context_;
  DeclId declId_;
  EntityId entityId_;

  llvm::Function *function_ = nullptr;
  const Type *returnType_ = nullptr;
  llvm::IRBuilder<> builder_;
  llvm::IRBuilder<> allocaBuilder_;
  std::unordered_map<uint32_t, llvm::AllocaInst *> locals_; // EntityId
  std::unordered_map<uint32_t, llvm::BasicBlock *> labels_; // LabelId
  std::vector<LoopContext> loops_;

  void collectLabels(StmtId id);
  void ensureInsertionPoint();
  bool currentBlockTerminated() const;
  llvm::AllocaInst *createAlloca(const Type *type, Symbol name);
  llvm::Value *addressOfEntity(EntityId id);

  void emitStmt(StmtId id);
  void emitCompound(StmtId id);
  void emitLabel(const LabelStmt &label);
  void emitLocalDecl(DeclId id);
  void emitDiscarded(ExprId id);

  ExprResult emitExpr(ExprId id);
  Place emitPlace(ExprId id);
  RValue emitValue(ExprId id);
  RValue toRValue(ExprResult result);
  RValue load(Place place);
  RValue decay(Place place);
  RValue decay(FunctionDesignator designator);
  RValue convert(RValue value, const Type *to, bool nullPointerConstant);
  RValue convertExpr(ExprId id, const Type *to);
  RValue emitInt(ExprId id);
  RValue emitPointerValue(ExprId id);
  llvm::Value *toBool(ExprId id);

  ExprResult emitAssignment(const AssignExpr &assign);
  RValue emitBinary(ExprId id, const BinaryExpr &binary);
  RValue emitPointerOffset(ExprId pointerId, ExprId indexId, bool negate,
                           const char *name);
  RValue emitLogical(const BinaryExpr &binary, bool isAnd);
  ExprResult emitConditional(ExprId id, const ConditionalExpr &conditional);
  ExprResult emitCall(ExprId id, const CallExpr &call);
};

void CodeGen::emit() {
  createExternalEntities();

  for (DeclId id : ast_.translationUnit().decls) {
    auto *function = std::get_if<FunctionDecl>(&ast_.decl(id).node);
    if (function && function->body.valid()) {
      FunctionEmitter emitter(*this, id);
      emitter.emit();
    }
  }

  std::string error;
  llvm::raw_string_ostream stream(error);
  if (llvm::verifyModule(module_, &stream)) {
    stream.flush();
    fail("generated invalid LLVM IR: " + error);
  }
}

void CodeGen::FunctionEmitter::emit() {
  const Decl &declaration = ast_.decl(declId_);
  const auto *decl = std::get_if<FunctionDecl>(&declaration.node);
  if (!decl || !decl->body.valid())
    fail("function emitter without a definition");

  const Entity &entity = model_.entity(entityId_);
  if (!entity.type || !Type::isFunction(entity.type))
    fail("function entity '" + entityName(entity) + "' has non-function type");

  llvm::Function *function = codegen_.functionFor(entityId_);
  if (!function->empty())
    fail("function '" + entityName(entity) + "' emitted twice");
  if (entity.parameters.size() != function->arg_size())
    fail("function '" + entityName(entity) +
         "' declaration/definition parameter mismatch");

  function_ = function;
  returnType_ = entity.type->result();

  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context_, "entry", function_);
  builder_.SetInsertPoint(entry);
  allocaBuilder_.SetInsertPoint(entry, entry->begin());

  unsigned index = 0;
  for (llvm::Argument &argument : function_->args()) {
    DeclId paramId = entity.parameters[index];
    EntityId paramEntityId = model_.decl(paramId).entity;
    if (!paramEntityId.valid())
      fail("function parameter without an entity");
    const Entity &param = model_.entity(paramEntityId);
    llvm::AllocaInst *slot = createAlloca(param.type, param.name);
    allocaBuilder_.CreateStore(&argument, slot);
    if (!locals_.emplace(paramEntityId.index, slot).second)
      fail("duplicate storage for parameter '" + entityName(param) + "'");
    argument.setName(param.name ? *param.name : "arg");
    ++index;
  }

  collectLabels(decl->body);
  emitStmt(decl->body);

  if (!currentBlockTerminated()) {
    if (Type::isVoid(returnType_))
      builder_.CreateRetVoid();
    else
      builder_.CreateRet(
          llvm::Constant::getNullValue(layout_.llvmType(returnType_)));
  }
}

void CodeGen::FunctionEmitter::collectLabels(StmtId id) {
  const Stmt &stmt = ast_.stmt(id);
  if (auto *compound = std::get_if<CompoundStmt>(&stmt.node)) {
    for (StmtId item : compound->items)
      collectLabels(item);
    return;
  }
  if (auto *label = std::get_if<LabelStmt>(&stmt.node)) {
    const LabelDecl &decl = ast_.label(label->label);
    if (labels_.count(label->label.index))
      fail("duplicate label block");
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

void CodeGen::FunctionEmitter::ensureInsertionPoint() {
  llvm::BasicBlock *block = builder_.GetInsertBlock();
  if (!block)
    fail("no insertion point");
  if (block->empty() || !block->back().isTerminator())
    return;
  llvm::BasicBlock *next =
      llvm::BasicBlock::Create(context_, "dead", function_);
  builder_.SetInsertPoint(next);
}

bool CodeGen::FunctionEmitter::currentBlockTerminated() const {
  llvm::BasicBlock *block = builder_.GetInsertBlock();
  return block && !block->empty() && block->back().isTerminator();
}

llvm::AllocaInst *CodeGen::FunctionEmitter::createAlloca(const Type *type,
                                                         Symbol name) {
  if (!function_)
    fail("alloca outside of a function");
  if (!type || !Type::isObject(type))
    fail("alloca of a non-object type");
  // Locals may be declared after the entry block already has a terminator
  // (for example inside an if or while body), so always insert new allocas
  // before the first instruction of the entry block rather than appending.
  llvm::BasicBlock *entry = &function_->getEntryBlock();
  allocaBuilder_.SetInsertPoint(entry, entry->getFirstInsertionPt());
  return allocaBuilder_.CreateAlloca(layout_.llvmType(type), nullptr,
                                     name ? *name : "tmp");
}

llvm::Value *CodeGen::FunctionEmitter::addressOfEntity(EntityId id) {
  if (!id.valid())
    fail("invalid entity id");
  const Entity &entity = model_.entity(id);
  if (entity.kind == EntityKind::Function)
    return codegen_.functionFor(id);
  if (entity.linkage == Linkage::External)
    return codegen_.globalFor(id);

  // A local object must have been materialized by its declaration.  Falling
  // back to a global here would silently invent a different semantic entity.
  auto it = locals_.find(id.index);
  if (it == locals_.end())
    fail("no storage for local entity '" + entityName(entity) + "'");
  return it->second;
}

void CodeGen::FunctionEmitter::emitCompound(StmtId id) {
  auto *compound = std::get_if<CompoundStmt>(&ast_.stmt(id).node);
  if (!compound)
    fail("expected a compound statement");
  for (StmtId item : compound->items)
    emitStmt(item);
}

void CodeGen::FunctionEmitter::emitLabel(const LabelStmt &label) {
  auto it = labels_.find(label.label.index);
  if (it == labels_.end())
    fail("missing label block");
  llvm::BasicBlock *block = it->second;
  if (!currentBlockTerminated())
    builder_.CreateBr(block);
  builder_.SetInsertPoint(block);
  emitStmt(label.stmt);
}

void CodeGen::FunctionEmitter::emitLocalDecl(DeclId id) {
  auto *var = std::get_if<VarDecl>(&ast_.decl(id).node);
  if (!var)
    return;
  EntityId entityId = model_.decl(id).entity;
  if (!entityId.valid())
    fail("local declaration without an entity");
  const Entity &entity = model_.entity(entityId);
  if (entity.kind != EntityKind::Object || entity.linkage != Linkage::None)
    return;
  if (locals_.count(entityId.index))
    fail("duplicate local storage for '" + entityName(entity) + "'");
  locals_[entityId.index] = createAlloca(entity.type, entity.name);
}

void CodeGen::FunctionEmitter::emitStmt(StmtId id) {
  const Stmt &stmt = ast_.stmt(id);

  if (auto *label = std::get_if<LabelStmt>(&stmt.node)) {
    emitLabel(*label);
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
      emitDiscarded(exprStmt->expr);
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
      if (Type::isVoid(returnType_))
        fail("return with a value in a void function");
      RValue value = convertExpr(returnStmt->expr, returnType_);
      builder_.CreateRet(value.value);
    } else {
      if (!Type::isVoid(returnType_))
        fail("return without a value in a non-void function");
      builder_.CreateRetVoid();
    }
    return;
  }

  if (std::holds_alternative<BreakStmt>(stmt.node)) {
    if (loops_.empty())
      fail("break outside of a loop");
    builder_.CreateBr(loops_.back().breakBlock);
    return;
  }

  if (std::holds_alternative<ContinueStmt>(stmt.node)) {
    if (loops_.empty())
      fail("continue outside of a loop");
    builder_.CreateBr(loops_.back().continueBlock);
    return;
  }

  if (std::holds_alternative<GotoStmt>(stmt.node)) {
    LabelId target = model_.gotoTarget(id);
    if (!target.valid())
      fail("goto without a resolved target");
    auto it = labels_.find(target.index);
    if (it == labels_.end())
      fail("missing label block for goto");
    builder_.CreateBr(it->second);
    return;
  }

  fail("unsupported statement in code generation");
}

CodeGen::FunctionEmitter::ExprResult
CodeGen::FunctionEmitter::emitExpr(ExprId id) {
  const ExprInfo &info = model_.expr(id);
  const Expr &expr = ast_.expr(id);

  // Sema folds integer constants and sizeof.  Returning before dispatch also
  // guarantees that a sizeof operand is never evaluated.
  if (info.isConstant) {
    if (!Type::isInteger(info.type))
      fail("constant expression with a non-integer type");
    return RValue{llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_),
                                         static_cast<uint64_t>(info.value)),
                  types_.intType()};
  }

  if (std::holds_alternative<IdentifierExpr>(expr.node)) {
    if (!info.entity.valid())
      fail("identifier without a resolved entity");
    const Entity &entity = model_.entity(info.entity);
    if (entity.kind == EntityKind::Function) {
      if (!entity.type || !Type::isFunction(entity.type))
        fail("function entity '" + entityName(entity) +
             "' has non-function type");
      return FunctionDesignator{codegen_.functionFor(info.entity), entity.type};
    }
    if (!entity.type || !Type::isObject(entity.type))
      fail("object entity '" + entityName(entity) + "' has non-object type");
    return Place{addressOfEntity(info.entity), entity.type};
  }

  if (std::holds_alternative<StringLiteralExpr>(expr.node)) {
    if (!Type::isArray(info.type))
      fail("string literal without array type");
    return Place{codegen_.stringLiteral(info), info.type};
  }

  if (auto *unary = std::get_if<UnaryExpr>(&expr.node)) {
    switch (unary->op) {
    case UnaryExpr::Op::Plus:
      return emitInt(unary->operand);
    case UnaryExpr::Op::Minus: {
      RValue operand = emitInt(unary->operand);
      return RValue{builder_.CreateNeg(operand.value, "neg"), types_.intType()};
    }
    case UnaryExpr::Op::Not: {
      llvm::Value *value = toBool(unary->operand);
      llvm::Value *negated =
          builder_.CreateXor(value, llvm::ConstantInt::getTrue(context_));
      return RValue{
          builder_.CreateZExt(negated, llvm::Type::getInt32Ty(context_), "not"),
          types_.intType()};
    }
    case UnaryExpr::Op::Deref: {
      RValue pointer = emitValue(unary->operand);
      if (!Type::isPointer(pointer.type))
        fail("dereference of a non-pointer value");
      if (!pointer.value->getType()->isPointerTy())
        fail("dereference of a non-pointer LLVM value");
      const Type *pointee = info.type;
      if (!pointee || pointee != pointer.type->pointee())
        fail("dereference type disagrees with pointer element type");
      if (Type::isFunction(pointee))
        return FunctionDesignator{pointer.value, pointee};
      return Place{pointer.value, pointee};
    }
    case UnaryExpr::Op::Addr: {
      ExprResult operand = emitExpr(unary->operand);
      if (auto *place = std::get_if<Place>(&operand))
        return RValue{place->address, info.type};
      if (auto *designator = std::get_if<FunctionDesignator>(&operand))
        return RValue{designator->callee, info.type};
      fail("cannot take the address of a value");
    }
    }
  }

  if (auto *binary = std::get_if<BinaryExpr>(&expr.node))
    return emitBinary(id, *binary);

  if (auto *assign = std::get_if<AssignExpr>(&expr.node))
    return emitAssignment(*assign);

  if (auto *conditional = std::get_if<ConditionalExpr>(&expr.node))
    return emitConditional(id, *conditional);

  if (auto *call = std::get_if<CallExpr>(&expr.node))
    return emitCall(id, *call);

  if (auto *subscript = std::get_if<SubscriptExpr>(&expr.node)) {
    const ExprInfo &baseInfo = model_.expr(subscript->base);
    RValue pointer;
    RValue index;
    if (Type::isPointer(baseInfo.type) || Type::isArray(baseInfo.type)) {
      pointer = emitValue(subscript->base);
      index = emitInt(subscript->index);
    } else {
      pointer = emitValue(subscript->index);
      index = emitInt(subscript->base);
    }
    if (!Type::isPointer(pointer.type))
      fail("subscript base is not a pointer");
    if (!pointer.value->getType()->isPointerTy())
      fail("subscript base is not a pointer LLVM value");
    if (!Type::isInteger(index.type))
      fail("subscript index is not an integer");
    if (!info.type || info.type != pointer.type->pointee())
      fail("subscript type disagrees with pointer element type");
    llvm::Value *offset = builder_.CreateSExt(
        index.value, llvm::Type::getInt64Ty(context_), "idx");
    llvm::Value *indices[] = {offset};
    llvm::Value *address =
        builder_.CreateGEP(layout_.llvmType(pointer.type->pointee()),
                           pointer.value, indices, "index");
    return Place{address, info.type};
  }

  if (auto *member = std::get_if<MemberExpr>(&expr.node)) {
    llvm::Value *base = nullptr;
    if (member->arrow) {
      RValue pointer = emitValue(member->base);
      if (!Type::isPointer(pointer.type))
        fail("arrow member base is not a pointer");
      if (!pointer.value->getType()->isPointerTy())
        fail("arrow member base is not a pointer LLVM value");
      base = pointer.value;
    } else {
      Place place = emitPlace(member->base);
      if (!Type::isStruct(place.type))
        fail("member base is not a struct");
      base = place.address;
    }
    if (!info.memberRecord)
      fail("member access without a record");
    llvm::Value *address =
        builder_.CreateStructGEP(layout_.llvmStruct(info.memberRecord), base,
                                 info.memberIndex, "member");
    return Place{address, info.type};
  }

  if (std::holds_alternative<SizeofExpr>(expr.node))
    return RValue{llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_),
                                         static_cast<uint64_t>(info.value)),
                  types_.intType()};

  fail("unsupported expression in code generation");
}

CodeGen::FunctionEmitter::Place CodeGen::FunctionEmitter::emitPlace(ExprId id) {
  ExprResult result = emitExpr(id);
  if (auto *place = std::get_if<Place>(&result))
    return *place;
  fail("expression does not denote an object");
}

CodeGen::FunctionEmitter::RValue
CodeGen::FunctionEmitter::emitValue(ExprId id) {
  return toRValue(emitExpr(id));
}

CodeGen::FunctionEmitter::RValue
CodeGen::FunctionEmitter::toRValue(ExprResult result) {
  if (auto *value = std::get_if<RValue>(&result))
    return *value;
  if (auto *place = std::get_if<Place>(&result)) {
    if (Type::isArray(place->type))
      return decay(*place);
    if (Type::isFunction(place->type))
      fail("function object in a value context");
    if (Type::isStruct(place->type))
      fail("struct value is not supported");
    if (!Type::isScalar(place->type))
      fail("non-scalar object in a value context");
    return load(*place);
  }
  if (auto *designator = std::get_if<FunctionDesignator>(&result))
    return decay(*designator);
  fail("void expression used as a value");
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::load(Place place) {
  if (!Type::isScalar(place.type))
    fail("load of a non-scalar object");
  if (!place.address || !place.address->getType()->isPointerTy())
    fail("load from a non-pointer address");
  return RValue{
      builder_.CreateLoad(layout_.llvmType(place.type), place.address),
      place.type};
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::decay(Place place) {
  if (!Type::isArray(place.type))
    fail("decay of a non-array place");
  if (!place.address || !place.address->getType()->isPointerTy())
    fail("decay of a non-pointer address");
  return RValue{place.address, types_.pointerTo(place.type->element())};
}

CodeGen::FunctionEmitter::RValue
CodeGen::FunctionEmitter::decay(FunctionDesignator designator) {
  if (!Type::isFunction(designator.type))
    fail("decay of a non-function designator");
  if (!designator.callee || !designator.callee->getType()->isPointerTy())
    fail("decay of a non-pointer callee");
  return RValue{designator.callee, types_.pointerTo(designator.type)};
}

CodeGen::FunctionEmitter::RValue
CodeGen::FunctionEmitter::convert(RValue value, const Type *to,
                                  bool nullPointerConstant) {
  if (!to)
    fail("conversion to a null type");
  if (!value.value)
    fail("conversion of a null LLVM value");

  if (Type::isInteger(to)) {
    if (!Type::isInteger(value.type))
      failConversion(value.type, to);
    llvm::Type *target = layout_.llvmType(to);
    if (value.value->getType() == target)
      return RValue{value.value, to};
    if (!value.value->getType()->isIntegerTy() || !target->isIntegerTy())
      failConversion(value.type, to);
    unsigned from = value.value->getType()->getIntegerBitWidth();
    unsigned dest = target->getIntegerBitWidth();
    if (from < dest)
      return RValue{builder_.CreateSExt(value.value, target), to};
    if (from > dest)
      return RValue{builder_.CreateTrunc(value.value, target), to};
    failConversion(value.type, to);
  }

  if (Type::isPointer(to)) {
    if (Type::isPointer(value.type)) {
      if (!value.value->getType()->isPointerTy())
        fail("pointer value has a non-pointer LLVM type");
      return RValue{value.value, to};
    }
    if (nullPointerConstant) {
      auto *constant = llvm::dyn_cast<llvm::ConstantInt>(value.value);
      if (!constant || !constant->isZero())
        fail("null pointer constant did not evaluate to zero");
      return RValue{llvm::ConstantPointerNull::get(opaquePointer(context_)),
                    to};
    }
    failConversion(value.type, to);
  }

  failConversion(value.type, to);
}

CodeGen::FunctionEmitter::RValue
CodeGen::FunctionEmitter::convertExpr(ExprId id, const Type *to) {
  return convert(emitValue(id), to, model_.expr(id).nullPointerConstant);
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::emitInt(ExprId id) {
  RValue value = emitValue(id);
  if (!Type::isInteger(value.type))
    fail("integer value required");
  return convert(value, types_.intType(), /*nullPointerConstant=*/false);
}

CodeGen::FunctionEmitter::RValue
CodeGen::FunctionEmitter::emitPointerValue(ExprId id) {
  if (model_.expr(id).nullPointerConstant) {
    const Type *pointerType = types_.pointerTo(types_.voidType());
    return RValue{llvm::ConstantPointerNull::get(opaquePointer(context_)),
                  pointerType};
  }
  RValue value = emitValue(id);
  if (!Type::isPointer(value.type))
    fail("pointer value required");
  if (!value.value->getType()->isPointerTy())
    fail("pointer value has a non-pointer LLVM type");
  return value;
}

llvm::Value *CodeGen::FunctionEmitter::toBool(ExprId id) {
  RValue value = emitValue(id);
  if (Type::isPointer(value.type)) {
    if (!value.value->getType()->isPointerTy())
      fail("pointer value has a non-pointer LLVM type");
    return builder_.CreateICmpNE(
        value.value, llvm::ConstantPointerNull::get(opaquePointer(context_)),
        "tobool");
  }
  if (Type::isInteger(value.type)) {
    if (!value.value->getType()->isIntegerTy())
      fail("integer value required");
    return builder_.CreateICmpNE(
        value.value, llvm::ConstantInt::get(value.value->getType(), 0),
        "tobool");
  }
  fail("scalar value required");
}

void CodeGen::FunctionEmitter::emitDiscarded(ExprId id) {
  ExprResult result = emitExpr(id);
  if (auto *place = std::get_if<Place>(&result)) {
    // C's lvalue conversion applies to scalar objects even when the value is
    // discarded; arrays and structs need no load here.
    if (Type::isScalar(place->type))
      (void)load(*place);
  }
}

CodeGen::FunctionEmitter::ExprResult
CodeGen::FunctionEmitter::emitAssignment(const AssignExpr &assign) {
  Place target = emitPlace(assign.lhs);
  if (!Type::isScalar(target.type))
    fail("assignment to a non-scalar object");
  RValue value = convertExpr(assign.rhs, target.type);
  builder_.CreateStore(value.value, target.address);
  return RValue{value.value, target.type};
}

CodeGen::FunctionEmitter::RValue
CodeGen::FunctionEmitter::emitPointerOffset(ExprId pointerId, ExprId indexId,
                                            bool negate, const char *name) {
  RValue pointer = emitValue(pointerId);
  if (!Type::isPointer(pointer.type))
    fail("pointer arithmetic requires a pointer operand");
  if (!pointer.value->getType()->isPointerTy())
    fail("pointer arithmetic on a non-pointer LLVM value");
  RValue index = emitInt(indexId);
  llvm::Value *offset =
      builder_.CreateSExt(index.value, llvm::Type::getInt64Ty(context_), "idx");
  if (negate)
    offset = builder_.CreateNeg(offset, "neg.idx");
  llvm::Value *indices[] = {offset};
  llvm::Value *address = builder_.CreateGEP(
      layout_.llvmType(pointer.type->pointee()), pointer.value, indices, name);
  return RValue{address, pointer.type};
}

CodeGen::FunctionEmitter::RValue
CodeGen::FunctionEmitter::emitBinary(ExprId id, const BinaryExpr &binary) {
  (void)id;
  const ExprInfo &leftInfo = model_.expr(binary.lhs);
  const ExprInfo &rightInfo = model_.expr(binary.rhs);
  const Type *leftType = leftInfo.type;
  const Type *rightType = rightInfo.type;

  switch (binary.op) {
  case BinaryExpr::Op::Mul: {
    if (!Type::isInteger(leftType) || !Type::isInteger(rightType))
      fail("'*' requires integer operands");
    RValue left = emitInt(binary.lhs);
    RValue right = emitInt(binary.rhs);
    return RValue{builder_.CreateMul(left.value, right.value, "mul"),
                  types_.intType()};
  }

  case BinaryExpr::Op::Add: {
    if (Type::isInteger(leftType) && Type::isInteger(rightType)) {
      RValue left = emitInt(binary.lhs);
      RValue right = emitInt(binary.rhs);
      return RValue{builder_.CreateAdd(left.value, right.value, "add"),
                    types_.intType()};
    }
    if ((Type::isPointer(leftType) || Type::isArray(leftType)) &&
        Type::isInteger(rightType))
      return emitPointerOffset(binary.lhs, binary.rhs, /*negate=*/false,
                               "add.ptr");
    if (Type::isInteger(leftType) &&
        (Type::isPointer(rightType) || Type::isArray(rightType)))
      return emitPointerOffset(binary.rhs, binary.lhs, /*negate=*/false,
                               "add.ptr");
    fail("invalid operands to binary '+'");
  }

  case BinaryExpr::Op::Sub: {
    if (Type::isInteger(leftType) && Type::isInteger(rightType)) {
      RValue left = emitInt(binary.lhs);
      RValue right = emitInt(binary.rhs);
      return RValue{builder_.CreateSub(left.value, right.value, "sub"),
                    types_.intType()};
    }
    if ((Type::isPointer(leftType) || Type::isArray(leftType)) &&
        Type::isInteger(rightType))
      return emitPointerOffset(binary.lhs, binary.rhs, /*negate=*/true,
                               "sub.ptr");
    if ((Type::isPointer(leftType) || Type::isArray(leftType)) &&
        (Type::isPointer(rightType) || Type::isArray(rightType))) {
      RValue left = emitValue(binary.lhs);
      if (!Type::isPointer(left.type))
        fail("pointer difference requires pointer operands");
      llvm::Value *leftInt = builder_.CreatePtrToInt(
          left.value, llvm::Type::getInt64Ty(context_), "left.int");
      RValue right = emitValue(binary.rhs);
      if (!Type::isPointer(right.type))
        fail("pointer difference requires pointer operands");
      llvm::Value *rightInt = builder_.CreatePtrToInt(
          right.value, llvm::Type::getInt64Ty(context_), "right.int");
      llvm::Value *difference =
          builder_.CreateSub(leftInt, rightInt, "ptr.diff");
      uint64_t size = layout_.sizeOf(left.type->pointee());
      if (size > 1)
        difference = builder_.CreateSDiv(
            difference,
            llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), size),
            "ptr.diff.scaled");
      return RValue{builder_.CreateTrunc(difference,
                                         llvm::Type::getInt32Ty(context_),
                                         "ptr.diff.int"),
                    types_.intType()};
    }
    fail("invalid operands to binary '-'");
  }

  case BinaryExpr::Op::Less: {
    if (Type::isInteger(leftType) && Type::isInteger(rightType)) {
      RValue left = emitInt(binary.lhs);
      RValue right = emitInt(binary.rhs);
      llvm::Value *result =
          builder_.CreateICmpSLT(left.value, right.value, "less");
      return RValue{builder_.CreateZExt(
                        result, llvm::Type::getInt32Ty(context_), "less.int"),
                    types_.intType()};
    }
    llvm::Value *left = emitPointerValue(binary.lhs).value;
    llvm::Value *right = emitPointerValue(binary.rhs).value;
    llvm::Value *result = builder_.CreateICmpULT(left, right, "less");
    return RValue{builder_.CreateZExt(result, llvm::Type::getInt32Ty(context_),
                                      "less.int"),
                  types_.intType()};
  }

  case BinaryExpr::Op::Equal:
  case BinaryExpr::Op::NotEqual: {
    bool equal = binary.op == BinaryExpr::Op::Equal;
    llvm::Value *left = nullptr;
    llvm::Value *right = nullptr;
    if (Type::isInteger(leftType) && Type::isInteger(rightType)) {
      left = emitInt(binary.lhs).value;
      right = emitInt(binary.rhs).value;
    } else {
      left = emitPointerValue(binary.lhs).value;
      right = emitPointerValue(binary.rhs).value;
    }
    llvm::Value *result = equal
                              ? builder_.CreateICmpEQ(left, right, "equal")
                              : builder_.CreateICmpNE(left, right, "notequal");
    return RValue{builder_.CreateZExt(result, llvm::Type::getInt32Ty(context_),
                                      equal ? "equal.int" : "notequal.int"),
                  types_.intType()};
  }

  case BinaryExpr::Op::LogicalAnd:
    return emitLogical(binary, /*isAnd=*/true);
  case BinaryExpr::Op::LogicalOr:
    return emitLogical(binary, /*isAnd=*/false);
  }

  fail("unsupported binary operator");
}

CodeGen::FunctionEmitter::RValue
CodeGen::FunctionEmitter::emitLogical(const BinaryExpr &binary, bool isAnd) {
  llvm::Value *left = toBool(binary.lhs);
  llvm::BasicBlock *leftBlock = builder_.GetInsertBlock();
  if (!leftBlock)
    fail("no insertion point for logical operand");

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
  if (!rightEnd)
    fail("no insertion point for logical operand");
  if (currentBlockTerminated())
    fail("logical operand terminated its block");
  builder_.CreateBr(endBlock);

  builder_.SetInsertPoint(endBlock);
  llvm::PHINode *phi =
      builder_.CreatePHI(llvm::Type::getInt1Ty(context_), 2, "logic");
  phi->addIncoming(isAnd ? llvm::ConstantInt::getFalse(context_)
                         : llvm::ConstantInt::getTrue(context_),
                   leftBlock);
  phi->addIncoming(right, rightEnd);
  return RValue{
      builder_.CreateZExt(phi, llvm::Type::getInt32Ty(context_), "logic.ext"),
      types_.intType()};
}

CodeGen::FunctionEmitter::ExprResult
CodeGen::FunctionEmitter::emitConditional(ExprId id,
                                          const ConditionalExpr &conditional) {
  const Type *type = model_.expr(id).type;
  if (!type)
    fail("conditional expression without a type");
  if (!Type::isVoid(type) && !Type::isScalar(type))
    fail("conditional expression has a non-scalar result");

  llvm::Value *condition = toBool(conditional.cond);
  llvm::BasicBlock *thenBlock =
      llvm::BasicBlock::Create(context_, "cond.then", function_);
  llvm::BasicBlock *elseBlock =
      llvm::BasicBlock::Create(context_, "cond.else", function_);
  llvm::BasicBlock *endBlock =
      llvm::BasicBlock::Create(context_, "cond.end", function_);
  builder_.CreateCondBr(condition, thenBlock, elseBlock);

  RValue thenValue;
  builder_.SetInsertPoint(thenBlock);
  if (Type::isVoid(type))
    emitDiscarded(conditional.thenExpr);
  else
    thenValue = convertExpr(conditional.thenExpr, type);
  llvm::BasicBlock *thenEnd = builder_.GetInsertBlock();
  if (!thenEnd || currentBlockTerminated())
    fail("conditional arm terminated its block");
  builder_.CreateBr(endBlock);

  RValue elseValue;
  builder_.SetInsertPoint(elseBlock);
  if (Type::isVoid(type))
    emitDiscarded(conditional.elseExpr);
  else
    elseValue = convertExpr(conditional.elseExpr, type);
  llvm::BasicBlock *elseEnd = builder_.GetInsertBlock();
  if (!elseEnd || currentBlockTerminated())
    fail("conditional arm terminated its block");
  builder_.CreateBr(endBlock);

  builder_.SetInsertPoint(endBlock);
  if (Type::isVoid(type))
    return VoidResult{};

  llvm::PHINode *phi = builder_.CreatePHI(layout_.llvmType(type), 2, "cond");
  phi->addIncoming(thenValue.value, thenEnd);
  phi->addIncoming(elseValue.value, elseEnd);
  return RValue{phi, type};
}

CodeGen::FunctionEmitter::ExprResult
CodeGen::FunctionEmitter::emitCall(ExprId id, const CallExpr &call) {
  (void)id;
  const ExprInfo &calleeInfo = model_.expr(call.callee);
  const Type *functionType = nullptr;
  if (Type::isFunction(calleeInfo.type))
    functionType = calleeInfo.type;
  else if (Type::isPointer(calleeInfo.type) &&
           Type::isFunction(calleeInfo.type->pointee()))
    functionType = calleeInfo.type->pointee();
  else
    fail("call to a non-function value");

  ExprResult calleeResult = emitExpr(call.callee);
  llvm::Value *callee = nullptr;
  if (auto *designator = std::get_if<FunctionDesignator>(&calleeResult)) {
    if (designator->type != functionType)
      fail("function designator type mismatch");
    callee = designator->callee;
  } else {
    RValue value = toRValue(calleeResult);
    if (!Type::isPointer(value.type))
      fail("call through a non-pointer callee");
    callee = value.value;
  }
  if (!callee || !callee->getType()->isPointerTy())
    fail("callee is not a pointer");

  std::vector<llvm::Value *> arguments;
  arguments.reserve(call.args.size());
  for (size_t i = 0; i < call.args.size(); ++i) {
    if (i >= functionType->params().size())
      fail("call has more arguments than parameters");
    arguments.push_back(
        convertExpr(call.args[i], functionType->params()[i]).value);
  }
  if (arguments.size() != functionType->params().size())
    fail("call argument count mismatch");

  llvm::FunctionType *llvmFunctionType =
      llvm::cast<llvm::FunctionType>(layout_.llvmType(functionType));
  llvm::CallInst *callInst =
      builder_.CreateCall(llvmFunctionType, callee, arguments);

  if (Type::isVoid(functionType->result()))
    return VoidResult{};
  if (!Type::isScalar(functionType->result()))
    fail("function result is not a scalar value");
  return RValue{callInst, functionType->result()};
}

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

llvm::PointerType* opaque_pointer(llvm::LLVMContext& context) {
    return llvm::PointerType::get(context, 0);
}

/// Internal code-generation failures are compiler bugs, not source-language
/// diagnostics.  They become "c4: internal error: ..." in main and exit 2.
[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error("codegen: " + message);
}

[[noreturn]] void fail_conversion(const Type* from, const Type* to) {
    fail("cannot convert '" + TypeContext::to_string(from) + "' to '" + TypeContext::to_string(to) +
         "'");
}

std::string entity_name(const Entity& entity) {
    return entity.name ? *entity.name : std::string("<anonymous>");
}

} // namespace

CodeGen::CodeGen(const AST& ast, const SemanticModel& model, TypeContext& types, TypeLayout& layout,
                 llvm::LLVMContext& context, llvm::Module& module)
    : ast_(ast), model_(model), types_(types), layout_(layout), context_(context), module_(module) {
}

void CodeGen::create_external_entities() {
    for (size_t i = 0; i < model_.entity_count(); ++i) {
        EntityId id{static_cast<uint32_t>(i)};
        const Entity& entity = model_.entity(id);
        if (entity.linkage != Linkage::External)
            continue;
        if (entity.kind == EntityKind::Function)
            function_for(id);
        else
            global_for(id);
    }
}

llvm::Function* CodeGen::function_for(EntityId id) {
    auto it = functions_.find(id.index);
    if (it != functions_.end())
        return it->second;

    const Entity& entity = model_.entity(id);
    if (entity.kind != EntityKind::Function)
        fail("entity '" + entity_name(entity) + "' is not a function");
    if (!entity.name)
        fail("external function without a name");
    if (!entity.type || !Type::is_function(entity.type))
        fail("function entity '" + entity_name(entity) + "' has non-function type");

    auto* function_type = llvm::cast<llvm::FunctionType>(layout_.llvm_type(entity.type));
    llvm::Function* function = llvm::Function::Create(
        function_type, llvm::GlobalValue::ExternalLinkage, *entity.name, &module_);
    functions_[id.index] = function;
    return function;
}

llvm::GlobalVariable* CodeGen::global_for(EntityId id) {
    auto it = globals_.find(id.index);
    if (it != globals_.end())
        return it->second;

    const Entity& entity = model_.entity(id);
    if (entity.kind != EntityKind::Object)
        fail("entity '" + entity_name(entity) + "' is not an object");
    if (!entity.name)
        fail("external object without a name");
    if (!entity.type || !Type::is_object(entity.type))
        fail("object entity '" + entity_name(entity) + "' has non-object type");

    llvm::Type* llvm_type = layout_.llvm_type(entity.type);
    auto* global = new llvm::GlobalVariable(module_, llvm_type, /*isConstant=*/false,
                                            llvm::GlobalValue::ExternalLinkage,
                                            llvm::Constant::getNullValue(llvm_type), *entity.name);
    globals_[id.index] = global;
    return global;
}

llvm::GlobalVariable* CodeGen::string_literal(const ExprInfo& info) {
    std::string data(info.string_bytes.begin(), info.string_bytes.end());
    llvm::Constant* initializer =
        llvm::ConstantDataArray::getString(context_, data, /*AddNull=*/true);
    llvm::ArrayType* array_type =
        llvm::ArrayType::get(llvm::Type::getInt8Ty(context_), data.size() + 1);
    return new llvm::GlobalVariable(module_, array_type, /*isConstant=*/true,
                                    llvm::GlobalValue::PrivateLinkage, initializer,
                                    ".str." + std::to_string(string_counter_++));
}

/// Lowers one function definition.  Every piece of function-specific state
/// (current function, return type, builders, locals, labels and loops) lives
/// here, so two consecutive definitions cannot share or leak state.
class CodeGen::FunctionEmitter {
public:
    FunctionEmitter(CodeGen& codegen, DeclId decl_id)
        : codegen_(codegen), ast_(codegen.ast_), model_(codegen.model_), types_(codegen.types_),
          layout_(codegen.layout_), context_(codegen.context_), decl_id_(decl_id),
          builder_(codegen.context_), alloca_builder_(codegen.context_) {
        entity_id_ = model_.decl(decl_id_).entity;
    }

    void emit();

private:
    /// An addressable object (or string-literal array) and its C4 type.
    struct Place {
        llvm::Value* address = nullptr;
        const Type* type = nullptr;
    };

    /// A computed scalar value (integer or pointer) and its C4 type.  Void,
    /// struct, array and function values are deliberately not representable.
    struct RValue {
        llvm::Value* value = nullptr;
        const Type* type = nullptr;
    };

    /// A callable entity.  callee is either an llvm::Function (direct call) or
    /// a pointer value (indirect call); type is the C4 function type.
    struct FunctionDesignator {
        llvm::Value* callee = nullptr;
        const Type* type = nullptr;
    };

    /// Evaluation with no resulting value (void call or void conditional).
    struct VoidResult {};

    using ExprResult = std::variant<Place, RValue, FunctionDesignator, VoidResult>;

    struct LoopContext {
        llvm::BasicBlock* break_block = nullptr;
        llvm::BasicBlock* continue_block = nullptr;
    };

    CodeGen& codegen_;
    const AST& ast_;
    const SemanticModel& model_;
    TypeContext& types_;
    TypeLayout& layout_;
    llvm::LLVMContext& context_;
    DeclId decl_id_;
    EntityId entity_id_;

    llvm::Function* function_ = nullptr;
    const Type* return_type_ = nullptr;
    llvm::IRBuilder<> builder_;
    llvm::IRBuilder<> alloca_builder_;
    std::unordered_map<uint32_t, llvm::AllocaInst*> locals_; // EntityId
    std::unordered_map<uint32_t, llvm::BasicBlock*> labels_; // LabelId
    std::vector<LoopContext> loops_;

    void collect_labels(StmtId id);
    void ensure_insertion_point();
    bool current_block_terminated() const;
    llvm::AllocaInst* create_alloca(const Type* type, Symbol name);
    llvm::Value* address_of_entity(EntityId id);

    void emit_stmt(StmtId id);
    void emit_compound(StmtId id);
    void emit_label(const LabelStmt& label);
    void emit_local_decl(DeclId id);
    void emit_discarded(ExprId id);

    ExprResult emit_expr(ExprId id);
    Place emit_place(ExprId id);
    RValue emit_value(ExprId id);
    RValue to_r_value(ExprResult result);
    RValue load(Place place);
    RValue decay(Place place);
    RValue decay(FunctionDesignator designator);
    RValue convert(RValue value, const Type* to, bool null_pointer_constant);
    RValue convert_expr(ExprId id, const Type* to);
    RValue emit_int(ExprId id);
    RValue emit_pointer_value(ExprId id);
    llvm::Value* to_bool(ExprId id);

    ExprResult emit_assignment(const AssignExpr& assign);
    RValue emit_binary(ExprId id, const BinaryExpr& binary);
    RValue emit_pointer_offset(ExprId pointer_id, ExprId index_id, bool negate, const char* name);
    RValue emit_logical(const BinaryExpr& binary, bool is_and);
    ExprResult emit_conditional(ExprId id, const ConditionalExpr& conditional);
    ExprResult emit_call(ExprId id, const CallExpr& call);
};

void CodeGen::emit() {
    create_external_entities();

    for (DeclId id : ast_.translation_unit().decls) {
        auto* function = std::get_if<FunctionDecl>(&ast_.decl(id).node);
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
    const Decl& declaration = ast_.decl(decl_id_);
    const auto* decl = std::get_if<FunctionDecl>(&declaration.node);
    if (!decl || !decl->body.valid())
        fail("function emitter without a definition");

    const Entity& entity = model_.entity(entity_id_);
    if (!entity.type || !Type::is_function(entity.type))
        fail("function entity '" + entity_name(entity) + "' has non-function type");

    llvm::Function* function = codegen_.function_for(entity_id_);
    if (!function->empty())
        fail("function '" + entity_name(entity) + "' emitted twice");
    if (entity.parameters.size() != function->arg_size())
        fail("function '" + entity_name(entity) + "' declaration/definition parameter mismatch");

    function_ = function;
    return_type_ = entity.type->result();

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", function_);
    builder_.SetInsertPoint(entry);
    alloca_builder_.SetInsertPoint(entry, entry->begin());

    unsigned index = 0;
    for (llvm::Argument& argument : function_->args()) {
        DeclId param_id = entity.parameters[index];
        EntityId param_entity_id = model_.decl(param_id).entity;
        if (!param_entity_id.valid())
            fail("function parameter without an entity");
        const Entity& param = model_.entity(param_entity_id);
        llvm::AllocaInst* slot = create_alloca(param.type, param.name);
        alloca_builder_.CreateStore(&argument, slot);
        if (!locals_.emplace(param_entity_id.index, slot).second)
            fail("duplicate storage for parameter '" + entity_name(param) + "'");
        argument.setName(param.name ? *param.name : "arg");
        ++index;
    }

    collect_labels(decl->body);
    emit_stmt(decl->body);

    if (!current_block_terminated()) {
        if (Type::is_void(return_type_))
            builder_.CreateRetVoid();
        else
            builder_.CreateRet(llvm::Constant::getNullValue(layout_.llvm_type(return_type_)));
    }
}

void CodeGen::FunctionEmitter::collect_labels(StmtId id) {
    const Stmt& stmt = ast_.stmt(id);
    if (auto* compound = std::get_if<CompoundStmt>(&stmt.node)) {
        for (StmtId item : compound->items)
            collect_labels(item);
        return;
    }
    if (auto* label = std::get_if<LabelStmt>(&stmt.node)) {
        const LabelDecl& decl = ast_.label(label->label);
        if (labels_.count(label->label.index))
            fail("duplicate label block");
        labels_[label->label.index] =
            llvm::BasicBlock::Create(context_, decl.name ? *decl.name : "label", function_);
        collect_labels(label->stmt);
        return;
    }
    if (auto* if_stmt = std::get_if<IfStmt>(&stmt.node)) {
        collect_labels(if_stmt->then_stmt);
        if (if_stmt->else_stmt.valid())
            collect_labels(if_stmt->else_stmt);
        return;
    }
    if (auto* while_stmt = std::get_if<WhileStmt>(&stmt.node))
        collect_labels(while_stmt->body);
}

void CodeGen::FunctionEmitter::ensure_insertion_point() {
    llvm::BasicBlock* block = builder_.GetInsertBlock();
    if (!block)
        fail("no insertion point");
    if (block->empty() || !block->back().isTerminator())
        return;
    llvm::BasicBlock* next = llvm::BasicBlock::Create(context_, "dead", function_);
    builder_.SetInsertPoint(next);
}

bool CodeGen::FunctionEmitter::current_block_terminated() const {
    llvm::BasicBlock* block = builder_.GetInsertBlock();
    return block && !block->empty() && block->back().isTerminator();
}

llvm::AllocaInst* CodeGen::FunctionEmitter::create_alloca(const Type* type, Symbol name) {
    if (!function_)
        fail("alloca outside of a function");
    if (!type || !Type::is_object(type))
        fail("alloca of a non-object type");
    // Locals may be declared after the entry block already has a terminator
    // (for example inside an if or while body), so always insert new allocas
    // before the first instruction of the entry block rather than appending.
    llvm::BasicBlock* entry = &function_->getEntryBlock();
    alloca_builder_.SetInsertPoint(entry, entry->getFirstInsertionPt());
    return alloca_builder_.CreateAlloca(layout_.llvm_type(type), nullptr, name ? *name : "tmp");
}

llvm::Value* CodeGen::FunctionEmitter::address_of_entity(EntityId id) {
    if (!id.valid())
        fail("invalid entity id");
    const Entity& entity = model_.entity(id);
    if (entity.kind == EntityKind::Function)
        return codegen_.function_for(id);
    if (entity.linkage == Linkage::External)
        return codegen_.global_for(id);

    // A local object must have been materialized by its declaration.  Falling
    // back to a global here would silently invent a different semantic entity.
    auto it = locals_.find(id.index);
    if (it == locals_.end())
        fail("no storage for local entity '" + entity_name(entity) + "'");
    return it->second;
}

void CodeGen::FunctionEmitter::emit_compound(StmtId id) {
    auto* compound = std::get_if<CompoundStmt>(&ast_.stmt(id).node);
    if (!compound)
        fail("expected a compound statement");
    for (StmtId item : compound->items)
        emit_stmt(item);
}

void CodeGen::FunctionEmitter::emit_label(const LabelStmt& label) {
    auto it = labels_.find(label.label.index);
    if (it == labels_.end())
        fail("missing label block");
    llvm::BasicBlock* block = it->second;
    if (!current_block_terminated())
        builder_.CreateBr(block);
    builder_.SetInsertPoint(block);
    emit_stmt(label.stmt);
}

void CodeGen::FunctionEmitter::emit_local_decl(DeclId id) {
    auto* var = std::get_if<VarDecl>(&ast_.decl(id).node);
    if (!var)
        return;
    EntityId entity_id = model_.decl(id).entity;
    if (!entity_id.valid())
        fail("local declaration without an entity");
    const Entity& entity = model_.entity(entity_id);
    if (entity.kind != EntityKind::Object || entity.linkage != Linkage::None)
        return;
    if (locals_.count(entity_id.index))
        fail("duplicate local storage for '" + entity_name(entity) + "'");
    locals_[entity_id.index] = create_alloca(entity.type, entity.name);
}

void CodeGen::FunctionEmitter::emit_stmt(StmtId id) {
    const Stmt& stmt = ast_.stmt(id);

    if (auto* label = std::get_if<LabelStmt>(&stmt.node)) {
        emit_label(*label);
        return;
    }

    ensure_insertion_point();

    if (std::holds_alternative<CompoundStmt>(stmt.node)) {
        emit_compound(id);
        return;
    }

    if (auto* decl_stmt = std::get_if<DeclStmt>(&stmt.node)) {
        emit_local_decl(decl_stmt->decl);
        return;
    }

    if (auto* expr_stmt = std::get_if<ExprStmt>(&stmt.node)) {
        if (expr_stmt->expr.valid())
            emit_discarded(expr_stmt->expr);
        return;
    }

    if (auto* if_stmt = std::get_if<IfStmt>(&stmt.node)) {
        llvm::Value* condition = to_bool(if_stmt->cond);
        llvm::BasicBlock* then_block = llvm::BasicBlock::Create(context_, "if.then", function_);
        llvm::BasicBlock* else_block =
            if_stmt->else_stmt.valid() ? llvm::BasicBlock::Create(context_, "if.else", function_)
                                       : nullptr;
        llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context_, "if.end", function_);
        builder_.CreateCondBr(condition, then_block, else_block ? else_block : end_block);

        builder_.SetInsertPoint(then_block);
        emit_stmt(if_stmt->then_stmt);
        if (!current_block_terminated())
            builder_.CreateBr(end_block);

        if (else_block) {
            builder_.SetInsertPoint(else_block);
            emit_stmt(if_stmt->else_stmt);
            if (!current_block_terminated())
                builder_.CreateBr(end_block);
        }

        builder_.SetInsertPoint(end_block);
        return;
    }

    if (auto* while_stmt = std::get_if<WhileStmt>(&stmt.node)) {
        llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context_, "while.cond", function_);
        llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context_, "while.body", function_);
        llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context_, "while.end", function_);
        builder_.CreateBr(cond_block);

        builder_.SetInsertPoint(cond_block);
        llvm::Value* condition = to_bool(while_stmt->cond);
        builder_.CreateCondBr(condition, body_block, end_block);

        builder_.SetInsertPoint(body_block);
        loops_.push_back(LoopContext{end_block, cond_block});
        emit_stmt(while_stmt->body);
        loops_.pop_back();
        if (!current_block_terminated())
            builder_.CreateBr(cond_block);

        builder_.SetInsertPoint(end_block);
        return;
    }

    if (auto* return_stmt = std::get_if<ReturnStmt>(&stmt.node)) {
        if (return_stmt->expr.valid()) {
            if (Type::is_void(return_type_))
                fail("return with a value in a void function");
            RValue value = convert_expr(return_stmt->expr, return_type_);
            builder_.CreateRet(value.value);
        } else {
            if (!Type::is_void(return_type_))
                fail("return without a value in a non-void function");
            builder_.CreateRetVoid();
        }
        return;
    }

    if (std::holds_alternative<BreakStmt>(stmt.node)) {
        if (loops_.empty())
            fail("break outside of a loop");
        builder_.CreateBr(loops_.back().break_block);
        return;
    }

    if (std::holds_alternative<ContinueStmt>(stmt.node)) {
        if (loops_.empty())
            fail("continue outside of a loop");
        builder_.CreateBr(loops_.back().continue_block);
        return;
    }

    if (std::holds_alternative<GotoStmt>(stmt.node)) {
        LabelId target = model_.goto_target(id);
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

CodeGen::FunctionEmitter::ExprResult CodeGen::FunctionEmitter::emit_expr(ExprId id) {
    const ExprInfo& info = model_.expr(id);
    const Expr& expr = ast_.expr(id);

    // Sema folds integer constants and sizeof.  Returning before dispatch also
    // guarantees that a sizeof operand is never evaluated.
    if (info.is_constant) {
        if (!Type::is_integer(info.type))
            fail("constant expression with a non-integer type");
        return RValue{llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_),
                                             static_cast<uint64_t>(info.value)),
                      types_.int_type()};
    }

    if (std::holds_alternative<IdentifierExpr>(expr.node)) {
        if (!info.entity.valid())
            fail("identifier without a resolved entity");
        const Entity& entity = model_.entity(info.entity);
        if (entity.kind == EntityKind::Function) {
            if (!entity.type || !Type::is_function(entity.type))
                fail("function entity '" + entity_name(entity) + "' has non-function type");
            return FunctionDesignator{codegen_.function_for(info.entity), entity.type};
        }
        if (!entity.type || !Type::is_object(entity.type))
            fail("object entity '" + entity_name(entity) + "' has non-object type");
        return Place{address_of_entity(info.entity), entity.type};
    }

    if (std::holds_alternative<StringLiteralExpr>(expr.node)) {
        if (!Type::is_array(info.type))
            fail("string literal without array type");
        return Place{codegen_.string_literal(info), info.type};
    }

    if (auto* unary = std::get_if<UnaryExpr>(&expr.node)) {
        switch (unary->op) {
            case UnaryExpr::Op::Plus:
                return emit_int(unary->operand);
            case UnaryExpr::Op::Minus: {
                RValue operand = emit_int(unary->operand);
                return RValue{builder_.CreateNeg(operand.value, "neg"), types_.int_type()};
            }
            case UnaryExpr::Op::Not: {
                llvm::Value* value = to_bool(unary->operand);
                llvm::Value* negated =
                    builder_.CreateXor(value, llvm::ConstantInt::getTrue(context_));
                return RValue{builder_.CreateZExt(negated, llvm::Type::getInt32Ty(context_), "not"),
                              types_.int_type()};
            }
            case UnaryExpr::Op::Deref: {
                RValue pointer = emit_value(unary->operand);
                if (!Type::is_pointer(pointer.type))
                    fail("dereference of a non-pointer value");
                if (!pointer.value->getType()->isPointerTy())
                    fail("dereference of a non-pointer LLVM value");
                const Type* pointee = info.type;
                if (!pointee || pointee != pointer.type->pointee())
                    fail("dereference type disagrees with pointer element type");
                if (Type::is_function(pointee))
                    return FunctionDesignator{pointer.value, pointee};
                return Place{pointer.value, pointee};
            }
            case UnaryExpr::Op::Addr: {
                ExprResult operand = emit_expr(unary->operand);
                if (auto* place = std::get_if<Place>(&operand))
                    return RValue{place->address, info.type};
                if (auto* designator = std::get_if<FunctionDesignator>(&operand))
                    return RValue{designator->callee, info.type};
                fail("cannot take the address of a value");
            }
        }
    }

    if (auto* binary = std::get_if<BinaryExpr>(&expr.node))
        return emit_binary(id, *binary);

    if (auto* assign = std::get_if<AssignExpr>(&expr.node))
        return emit_assignment(*assign);

    if (auto* conditional = std::get_if<ConditionalExpr>(&expr.node))
        return emit_conditional(id, *conditional);

    if (auto* call = std::get_if<CallExpr>(&expr.node))
        return emit_call(id, *call);

    if (auto* subscript = std::get_if<SubscriptExpr>(&expr.node)) {
        const ExprInfo& base_info = model_.expr(subscript->base);
        RValue pointer;
        RValue index;
        if (Type::is_pointer(base_info.type) || Type::is_array(base_info.type)) {
            pointer = emit_value(subscript->base);
            index = emit_int(subscript->index);
        } else {
            pointer = emit_value(subscript->index);
            index = emit_int(subscript->base);
        }
        if (!Type::is_pointer(pointer.type))
            fail("subscript base is not a pointer");
        if (!pointer.value->getType()->isPointerTy())
            fail("subscript base is not a pointer LLVM value");
        if (!Type::is_integer(index.type))
            fail("subscript index is not an integer");
        if (!info.type || info.type != pointer.type->pointee())
            fail("subscript type disagrees with pointer element type");
        llvm::Value* offset =
            builder_.CreateSExt(index.value, llvm::Type::getInt64Ty(context_), "idx");
        llvm::Value* indices[] = {offset};
        llvm::Value* address = builder_.CreateGEP(layout_.llvm_type(pointer.type->pointee()),
                                                  pointer.value, indices, "index");
        return Place{address, info.type};
    }

    if (auto* member = std::get_if<MemberExpr>(&expr.node)) {
        llvm::Value* base = nullptr;
        if (member->arrow) {
            RValue pointer = emit_value(member->base);
            if (!Type::is_pointer(pointer.type))
                fail("arrow member base is not a pointer");
            if (!pointer.value->getType()->isPointerTy())
                fail("arrow member base is not a pointer LLVM value");
            base = pointer.value;
        } else {
            Place place = emit_place(member->base);
            if (!Type::is_struct(place.type))
                fail("member base is not a struct");
            base = place.address;
        }
        if (!info.member_record)
            fail("member access without a record");
        llvm::Value* address = builder_.CreateStructGEP(layout_.llvm_struct(info.member_record),
                                                        base, info.member_index, "member");
        return Place{address, info.type};
    }

    if (std::holds_alternative<SizeofExpr>(expr.node))
        return RValue{llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_),
                                             static_cast<uint64_t>(info.value)),
                      types_.int_type()};

    fail("unsupported expression in code generation");
}

CodeGen::FunctionEmitter::Place CodeGen::FunctionEmitter::emit_place(ExprId id) {
    ExprResult result = emit_expr(id);
    if (auto* place = std::get_if<Place>(&result))
        return *place;
    fail("expression does not denote an object");
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::emit_value(ExprId id) {
    return to_r_value(emit_expr(id));
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::to_r_value(ExprResult result) {
    if (auto* value = std::get_if<RValue>(&result))
        return *value;
    if (auto* place = std::get_if<Place>(&result)) {
        if (Type::is_array(place->type))
            return decay(*place);
        if (Type::is_function(place->type))
            fail("function object in a value context");
        if (Type::is_struct(place->type))
            fail("struct value is not supported");
        if (!Type::is_scalar(place->type))
            fail("non-scalar object in a value context");
        return load(*place);
    }
    if (auto* designator = std::get_if<FunctionDesignator>(&result))
        return decay(*designator);
    fail("void expression used as a value");
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::load(Place place) {
    if (!Type::is_scalar(place.type))
        fail("load of a non-scalar object");
    if (!place.address || !place.address->getType()->isPointerTy())
        fail("load from a non-pointer address");
    return RValue{builder_.CreateLoad(layout_.llvm_type(place.type), place.address), place.type};
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::decay(Place place) {
    if (!Type::is_array(place.type))
        fail("decay of a non-array place");
    if (!place.address || !place.address->getType()->isPointerTy())
        fail("decay of a non-pointer address");
    return RValue{place.address, types_.pointer_to(place.type->element())};
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::decay(FunctionDesignator designator) {
    if (!Type::is_function(designator.type))
        fail("decay of a non-function designator");
    if (!designator.callee || !designator.callee->getType()->isPointerTy())
        fail("decay of a non-pointer callee");
    return RValue{designator.callee, types_.pointer_to(designator.type)};
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::convert(RValue value, const Type* to,
                                                                   bool null_pointer_constant) {
    if (!to)
        fail("conversion to a null type");
    if (!value.value)
        fail("conversion of a null LLVM value");

    if (Type::is_integer(to)) {
        if (!Type::is_integer(value.type))
            fail_conversion(value.type, to);
        llvm::Type* target = layout_.llvm_type(to);
        if (value.value->getType() == target)
            return RValue{value.value, to};
        if (!value.value->getType()->isIntegerTy() || !target->isIntegerTy())
            fail_conversion(value.type, to);
        unsigned from = value.value->getType()->getIntegerBitWidth();
        unsigned dest = target->getIntegerBitWidth();
        if (from < dest)
            return RValue{builder_.CreateSExt(value.value, target), to};
        if (from > dest)
            return RValue{builder_.CreateTrunc(value.value, target), to};
        fail_conversion(value.type, to);
    }

    if (Type::is_pointer(to)) {
        if (Type::is_pointer(value.type)) {
            if (!value.value->getType()->isPointerTy())
                fail("pointer value has a non-pointer LLVM type");
            return RValue{value.value, to};
        }
        if (null_pointer_constant) {
            auto* constant = llvm::dyn_cast<llvm::ConstantInt>(value.value);
            if (!constant || !constant->isZero())
                fail("null pointer constant did not evaluate to zero");
            return RValue{llvm::ConstantPointerNull::get(opaque_pointer(context_)), to};
        }
        fail_conversion(value.type, to);
    }

    fail_conversion(value.type, to);
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::convert_expr(ExprId id, const Type* to) {
    return convert(emit_value(id), to, model_.expr(id).null_pointer_constant);
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::emit_int(ExprId id) {
    RValue value = emit_value(id);
    if (!Type::is_integer(value.type))
        fail("integer value required");
    return convert(value, types_.int_type(), /*nullPointerConstant=*/false);
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::emit_pointer_value(ExprId id) {
    if (model_.expr(id).null_pointer_constant) {
        const Type* pointer_type = types_.pointer_to(types_.void_type());
        return RValue{llvm::ConstantPointerNull::get(opaque_pointer(context_)), pointer_type};
    }
    RValue value = emit_value(id);
    if (!Type::is_pointer(value.type))
        fail("pointer value required");
    if (!value.value->getType()->isPointerTy())
        fail("pointer value has a non-pointer LLVM type");
    return value;
}

llvm::Value* CodeGen::FunctionEmitter::to_bool(ExprId id) {
    RValue value = emit_value(id);
    if (Type::is_pointer(value.type)) {
        if (!value.value->getType()->isPointerTy())
            fail("pointer value has a non-pointer LLVM type");
        return builder_.CreateICmpNE(
            value.value, llvm::ConstantPointerNull::get(opaque_pointer(context_)), "tobool");
    }
    if (Type::is_integer(value.type)) {
        if (!value.value->getType()->isIntegerTy())
            fail("integer value required");
        return builder_.CreateICmpNE(value.value, llvm::ConstantInt::get(value.value->getType(), 0),
                                     "tobool");
    }
    fail("scalar value required");
}

void CodeGen::FunctionEmitter::emit_discarded(ExprId id) {
    ExprResult result = emit_expr(id);
    if (auto* place = std::get_if<Place>(&result)) {
        // C's lvalue conversion applies to scalar objects even when the value is
        // discarded; arrays and structs need no load here.
        if (Type::is_scalar(place->type))
            (void)load(*place);
    }
}

CodeGen::FunctionEmitter::ExprResult
CodeGen::FunctionEmitter::emit_assignment(const AssignExpr& assign) {
    Place target = emit_place(assign.lhs);
    if (!Type::is_scalar(target.type))
        fail("assignment to a non-scalar object");
    RValue value = convert_expr(assign.rhs, target.type);
    builder_.CreateStore(value.value, target.address);
    return RValue{value.value, target.type};
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::emit_pointer_offset(ExprId pointer_id,
                                                                               ExprId index_id,
                                                                               bool negate,
                                                                               const char* name) {
    RValue pointer = emit_value(pointer_id);
    if (!Type::is_pointer(pointer.type))
        fail("pointer arithmetic requires a pointer operand");
    if (!pointer.value->getType()->isPointerTy())
        fail("pointer arithmetic on a non-pointer LLVM value");
    RValue index = emit_int(index_id);
    llvm::Value* offset = builder_.CreateSExt(index.value, llvm::Type::getInt64Ty(context_), "idx");
    if (negate)
        offset = builder_.CreateNeg(offset, "neg.idx");
    llvm::Value* indices[] = {offset};
    llvm::Value* address = builder_.CreateGEP(layout_.llvm_type(pointer.type->pointee()),
                                              pointer.value, indices, name);
    return RValue{address, pointer.type};
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::emit_binary(ExprId id,
                                                                       const BinaryExpr& binary) {
    (void)id;
    const ExprInfo& left_info = model_.expr(binary.lhs);
    const ExprInfo& right_info = model_.expr(binary.rhs);
    const Type* left_type = left_info.type;
    const Type* right_type = right_info.type;

    switch (binary.op) {
        case BinaryExpr::Op::Mul: {
            if (!Type::is_integer(left_type) || !Type::is_integer(right_type))
                fail("'*' requires integer operands");
            RValue left = emit_int(binary.lhs);
            RValue right = emit_int(binary.rhs);
            return RValue{builder_.CreateMul(left.value, right.value, "mul"), types_.int_type()};
        }

        case BinaryExpr::Op::Add: {
            if (Type::is_integer(left_type) && Type::is_integer(right_type)) {
                RValue left = emit_int(binary.lhs);
                RValue right = emit_int(binary.rhs);
                return RValue{builder_.CreateAdd(left.value, right.value, "add"),
                              types_.int_type()};
            }
            if ((Type::is_pointer(left_type) || Type::is_array(left_type)) &&
                Type::is_integer(right_type))
                return emit_pointer_offset(binary.lhs, binary.rhs, /*negate=*/false, "add.ptr");
            if (Type::is_integer(left_type) &&
                (Type::is_pointer(right_type) || Type::is_array(right_type)))
                return emit_pointer_offset(binary.rhs, binary.lhs, /*negate=*/false, "add.ptr");
            fail("invalid operands to binary '+'");
        }

        case BinaryExpr::Op::Sub: {
            if (Type::is_integer(left_type) && Type::is_integer(right_type)) {
                RValue left = emit_int(binary.lhs);
                RValue right = emit_int(binary.rhs);
                return RValue{builder_.CreateSub(left.value, right.value, "sub"),
                              types_.int_type()};
            }
            if ((Type::is_pointer(left_type) || Type::is_array(left_type)) &&
                Type::is_integer(right_type))
                return emit_pointer_offset(binary.lhs, binary.rhs, /*negate=*/true, "sub.ptr");
            if ((Type::is_pointer(left_type) || Type::is_array(left_type)) &&
                (Type::is_pointer(right_type) || Type::is_array(right_type))) {
                RValue left = emit_value(binary.lhs);
                if (!Type::is_pointer(left.type))
                    fail("pointer difference requires pointer operands");
                llvm::Value* left_int = builder_.CreatePtrToInt(
                    left.value, llvm::Type::getInt64Ty(context_), "left.int");
                RValue right = emit_value(binary.rhs);
                if (!Type::is_pointer(right.type))
                    fail("pointer difference requires pointer operands");
                llvm::Value* right_int = builder_.CreatePtrToInt(
                    right.value, llvm::Type::getInt64Ty(context_), "right.int");
                llvm::Value* difference = builder_.CreateSub(left_int, right_int, "ptr.diff");
                uint64_t size = layout_.size_of(left.type->pointee());
                if (size > 1)
                    difference = builder_.CreateSDiv(
                        difference, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), size),
                        "ptr.diff.scaled");
                return RValue{builder_.CreateTrunc(difference, llvm::Type::getInt32Ty(context_),
                                                   "ptr.diff.int"),
                              types_.int_type()};
            }
            fail("invalid operands to binary '-'");
        }

        case BinaryExpr::Op::Less: {
            if (Type::is_integer(left_type) && Type::is_integer(right_type)) {
                RValue left = emit_int(binary.lhs);
                RValue right = emit_int(binary.rhs);
                llvm::Value* result = builder_.CreateICmpSLT(left.value, right.value, "less");
                return RValue{
                    builder_.CreateZExt(result, llvm::Type::getInt32Ty(context_), "less.int"),
                    types_.int_type()};
            }
            llvm::Value* left = emit_pointer_value(binary.lhs).value;
            llvm::Value* right = emit_pointer_value(binary.rhs).value;
            llvm::Value* result = builder_.CreateICmpULT(left, right, "less");
            return RValue{builder_.CreateZExt(result, llvm::Type::getInt32Ty(context_), "less.int"),
                          types_.int_type()};
        }

        case BinaryExpr::Op::Equal:
        case BinaryExpr::Op::NotEqual: {
            bool equal = binary.op == BinaryExpr::Op::Equal;
            llvm::Value* left = nullptr;
            llvm::Value* right = nullptr;
            if (Type::is_integer(left_type) && Type::is_integer(right_type)) {
                left = emit_int(binary.lhs).value;
                right = emit_int(binary.rhs).value;
            } else {
                left = emit_pointer_value(binary.lhs).value;
                right = emit_pointer_value(binary.rhs).value;
            }
            llvm::Value* result = equal ? builder_.CreateICmpEQ(left, right, "equal")
                                        : builder_.CreateICmpNE(left, right, "notequal");
            return RValue{builder_.CreateZExt(result, llvm::Type::getInt32Ty(context_),
                                              equal ? "equal.int" : "notequal.int"),
                          types_.int_type()};
        }

        case BinaryExpr::Op::LogicalAnd:
            return emit_logical(binary, /*isAnd=*/true);
        case BinaryExpr::Op::LogicalOr:
            return emit_logical(binary, /*isAnd=*/false);
    }

    fail("unsupported binary operator");
}

CodeGen::FunctionEmitter::RValue CodeGen::FunctionEmitter::emit_logical(const BinaryExpr& binary,
                                                                        bool is_and) {
    llvm::Value* left = to_bool(binary.lhs);
    llvm::BasicBlock* left_block = builder_.GetInsertBlock();
    if (!left_block)
        fail("no insertion point for logical operand");

    llvm::BasicBlock* right_block = llvm::BasicBlock::Create(context_, "logic.rhs", function_);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context_, "logic.end", function_);

    if (is_and)
        builder_.CreateCondBr(left, right_block, end_block);
    else
        builder_.CreateCondBr(left, end_block, right_block);

    builder_.SetInsertPoint(right_block);
    llvm::Value* right = to_bool(binary.rhs);
    llvm::BasicBlock* right_end = builder_.GetInsertBlock();
    if (!right_end)
        fail("no insertion point for logical operand");
    if (current_block_terminated())
        fail("logical operand terminated its block");
    builder_.CreateBr(end_block);

    builder_.SetInsertPoint(end_block);
    llvm::PHINode* phi = builder_.CreatePHI(llvm::Type::getInt1Ty(context_), 2, "logic");
    phi->addIncoming(is_and ? llvm::ConstantInt::getFalse(context_)
                            : llvm::ConstantInt::getTrue(context_),
                     left_block);
    phi->addIncoming(right, right_end);
    return RValue{builder_.CreateZExt(phi, llvm::Type::getInt32Ty(context_), "logic.ext"),
                  types_.int_type()};
}

CodeGen::FunctionEmitter::ExprResult
CodeGen::FunctionEmitter::emit_conditional(ExprId id, const ConditionalExpr& conditional) {
    const Type* type = model_.expr(id).type;
    if (!type)
        fail("conditional expression without a type");
    if (!Type::is_void(type) && !Type::is_scalar(type))
        fail("conditional expression has a non-scalar result");

    llvm::Value* condition = to_bool(conditional.cond);
    llvm::BasicBlock* then_block = llvm::BasicBlock::Create(context_, "cond.then", function_);
    llvm::BasicBlock* else_block = llvm::BasicBlock::Create(context_, "cond.else", function_);
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context_, "cond.end", function_);
    builder_.CreateCondBr(condition, then_block, else_block);

    RValue then_value;
    builder_.SetInsertPoint(then_block);
    if (Type::is_void(type))
        emit_discarded(conditional.then_expr);
    else
        then_value = convert_expr(conditional.then_expr, type);
    llvm::BasicBlock* then_end = builder_.GetInsertBlock();
    if (!then_end || current_block_terminated())
        fail("conditional arm terminated its block");
    builder_.CreateBr(end_block);

    RValue else_value;
    builder_.SetInsertPoint(else_block);
    if (Type::is_void(type))
        emit_discarded(conditional.else_expr);
    else
        else_value = convert_expr(conditional.else_expr, type);
    llvm::BasicBlock* else_end = builder_.GetInsertBlock();
    if (!else_end || current_block_terminated())
        fail("conditional arm terminated its block");
    builder_.CreateBr(end_block);

    builder_.SetInsertPoint(end_block);
    if (Type::is_void(type))
        return VoidResult{};

    llvm::PHINode* phi = builder_.CreatePHI(layout_.llvm_type(type), 2, "cond");
    phi->addIncoming(then_value.value, then_end);
    phi->addIncoming(else_value.value, else_end);
    return RValue{phi, type};
}

CodeGen::FunctionEmitter::ExprResult CodeGen::FunctionEmitter::emit_call(ExprId id,
                                                                         const CallExpr& call) {
    (void)id;
    const ExprInfo& callee_info = model_.expr(call.callee);
    const Type* function_type = nullptr;
    if (Type::is_function(callee_info.type))
        function_type = callee_info.type;
    else if (Type::is_pointer(callee_info.type) && Type::is_function(callee_info.type->pointee()))
        function_type = callee_info.type->pointee();
    else
        fail("call to a non-function value");

    ExprResult callee_result = emit_expr(call.callee);
    llvm::Value* callee = nullptr;
    if (auto* designator = std::get_if<FunctionDesignator>(&callee_result)) {
        if (designator->type != function_type)
            fail("function designator type mismatch");
        callee = designator->callee;
    } else {
        RValue value = to_r_value(callee_result);
        if (!Type::is_pointer(value.type))
            fail("call through a non-pointer callee");
        callee = value.value;
    }
    if (!callee || !callee->getType()->isPointerTy())
        fail("callee is not a pointer");

    std::vector<llvm::Value*> arguments;
    arguments.reserve(call.args.size());
    for (size_t i = 0; i < call.args.size(); ++i) {
        if (i >= function_type->params().size())
            fail("call has more arguments than parameters");
        arguments.push_back(convert_expr(call.args[i], function_type->params()[i]).value);
    }
    if (arguments.size() != function_type->params().size())
        fail("call argument count mismatch");

    llvm::FunctionType* llvm_function_type =
        llvm::cast<llvm::FunctionType>(layout_.llvm_type(function_type));
    llvm::CallInst* call_inst = builder_.CreateCall(llvm_function_type, callee, arguments);

    if (Type::is_void(function_type->result()))
        return VoidResult{};
    if (!Type::is_scalar(function_type->result()))
        fail("function result is not a scalar value");
    return RValue{call_inst, function_type->result()};
}

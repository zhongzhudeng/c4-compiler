#include "Sema.hpp"

#include <cassert>
#include <climits>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

namespace {

int32_t wrapInt(long long value) {
  return static_cast<int32_t>(static_cast<uint32_t>(value));
}

int escapeValue(char c, SourceLoc loc, Diagnostics &diagnostics) {
  switch (c) {
  case '\'':
    return '\'';
  case '"':
    return '"';
  case '?':
    return '?';
  case '\\':
    return '\\';
  case 'a':
    return 7;
  case 'b':
    return 8;
  case 'f':
    return 12;
  case 'n':
    return 10;
  case 'r':
    return 13;
  case 't':
    return 9;
  case 'v':
    return 11;
  default:
    diagnostics.error(loc, "invalid escape sequence");
  }
}

long long decodeCharacterConstant(Symbol spelling, SourceLoc loc,
                                  Diagnostics &diagnostics) {
  const std::string &text = *spelling;
  assert(text.size() >= 3 && text.front() == '\'' && text.back() == '\'');
  if (text.size() == 3) {
    // The execution character set is ISO-8859-1 and char is signed.
    unsigned char byte = static_cast<unsigned char>(text[1]);
    return static_cast<signed char>(byte);
  }
  assert(text.size() == 4 && text[1] == '\\');
  return escapeValue(text[2], loc, diagnostics);
}

long long decodeIntegerConstant(Symbol spelling, SourceLoc loc,
                                Diagnostics &diagnostics) {
  const std::string &text = *spelling;
  if (!text.empty() && text.front() == '\'')
    return decodeCharacterConstant(spelling, loc, diagnostics);

  long long value = 0;
  for (char c : text) {
    if (c < '0' || c > '9')
      diagnostics.error(loc, "invalid integer constant");
    value = value * 10 + (c - '0');
    if (value > INT_MAX)
      diagnostics.error(loc, "integer constant is too large");
  }
  return value;
}

std::vector<char> decodeStringLiteral(Symbol spelling, SourceLoc loc,
                                      Diagnostics &diagnostics) {
  const std::string &text = *spelling;
  assert(text.size() >= 2 && text.front() == '"' && text.back() == '"');
  std::vector<char> bytes;
  bytes.reserve(text.size() - 2);
  for (size_t i = 1; i + 1 < text.size(); ++i) {
    char c = text[i];
    if (c == '\\') {
      ++i;
      if (i + 1 > text.size())
        diagnostics.error(loc, "invalid escape sequence");
      bytes.push_back(static_cast<char>(escapeValue(text[i], loc, diagnostics)));
    } else {
      bytes.push_back(c);
    }
  }
  return bytes;
}

SourceLoc declarationNameLoc(const AST &ast, DeclId id) {
  const Decl &decl = ast.decl(id);
  DeclaratorId declarator;
  if (auto *var = std::get_if<VarDecl>(&decl.node))
    declarator = var->declarator;
  else if (auto *fun = std::get_if<FunctionDecl>(&decl.node))
    declarator = fun->declarator;
  else if (auto *field = std::get_if<FieldDecl>(&decl.node))
    declarator = field->declarator;
  else if (auto *param = std::get_if<ParamDecl>(&decl.node))
    declarator = param->declarator;
  else
    return decl.loc;
  SourceLoc loc = declaratorNameLoc(ast, declarator);
  return loc.line ? loc : decl.loc;
}

} // namespace

Sema::Sema(const AST &ast, SemanticModel &model, TypeContext &types,
           TypeLayout &layout, Diagnostics &diagnostics)
    : ast_(ast), model_(model), types_(types), layout_(layout),
      diagnostics_(diagnostics) {
  current_ = model_.fileScope();
}

void Sema::analyze() {
  for (DeclId id : ast_.translationUnit().decls)
    analyzeTopLevel(id);
  // File-scope tentative definitions may name an incomplete struct that is
  // completed later in the translation unit, so check them only now.
  for (size_t i = 0; i < model_.entityCount(); ++i) {
    EntityId id{static_cast<uint32_t>(i)};
    const Entity &entity = model_.entity(id);
    if (entity.kind == EntityKind::Object &&
        entity.definition == DefinitionState::Tentative)
      validateObjectType(entity.type, entity.loc, "variable");
  }
}

ScopeId Sema::pushScope(ScopeKind kind) {
  ScopeId scope = model_.addScope(kind, current_);
  current_ = scope;
  return scope;
}

void Sema::popScope() {
  ScopeId parent = model_.scope(current_).parent;
  assert(parent.valid() && "cannot pop the file scope");
  current_ = parent;
}

EntityId Sema::lookupOrdinary(Symbol name) const {
  return model_.lookup(current_, name);
}

StructDecl *Sema::lookupTag(Symbol name) const {
  return model_.lookupTag(current_, name);
}

void Sema::recordDeclaration(DeclId id, EntityId entity, ScopeId scope) {
  model_.entity(entity).declarations.push_back(id);
  DeclInfo &info = model_.decl(id);
  info.entity = entity;
  info.scope = scope;
}

EntityId Sema::declareFileObject(DeclId id, const DeclResult &result,
                                 SourceLoc loc) {
  ScopeId file = model_.fileScope();
  EntityId entity = model_.binding(file, result.name);

  if (entity.valid()) {
    const Entity &existing = model_.entity(entity);
    if (existing.kind != EntityKind::Object)
      diagnostics_.error(loc, "redefinition of '" + *result.name +
                                  "' as a different kind of symbol");
    if (existing.type != result.type)
      diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
  } else {
    auto external = externalEntities_.find(result.name);
    if (external != externalEntities_.end()) {
      // A block-scope function declaration created the external entity before
      // any file-scope declaration made the name visible there.
      entity = external->second;
      const Entity &existing = model_.entity(entity);
      if (existing.kind != EntityKind::Object)
        diagnostics_.error(loc, "redefinition of '" + *result.name +
                                    "' as a different kind of symbol");
      if (existing.type != result.type)
        diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
      model_.bind(file, result.name, entity);
    } else {
      entity = model_.addEntity(EntityKind::Object, result.name, result.type,
                                Linkage::External, loc);
      externalEntities_.emplace(result.name, entity);
      model_.bind(file, result.name, entity);
    }
  }

  model_.entity(entity).definition = DefinitionState::Tentative;
  recordDeclaration(id, entity, file);
  return entity;
}

EntityId Sema::declareFileFunction(DeclId id, const DeclResult &result,
                                   SourceLoc loc, bool isDefinition) {
  ScopeId file = model_.fileScope();
  EntityId entity = model_.binding(file, result.name);

  if (entity.valid()) {
    const Entity &existing = model_.entity(entity);
    if (existing.kind != EntityKind::Function)
      diagnostics_.error(loc, "redefinition of '" + *result.name +
                                  "' as a different kind of symbol");
    if (existing.type != result.type)
      diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
  } else {
    auto external = externalEntities_.find(result.name);
    if (external != externalEntities_.end()) {
      entity = external->second;
      const Entity &existing = model_.entity(entity);
      if (existing.kind != EntityKind::Function)
        diagnostics_.error(loc, "redefinition of '" + *result.name +
                                    "' as a different kind of symbol");
      if (existing.type != result.type)
        diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
      model_.bind(file, result.name, entity);
    } else {
      entity = model_.addEntity(EntityKind::Function, result.name, result.type,
                                Linkage::External, loc);
      externalEntities_.emplace(result.name, entity);
      model_.bind(file, result.name, entity);
    }
  }

  Entity &declared = model_.entity(entity);
  if (isDefinition) {
    if (declared.definition == DefinitionState::Defined)
      diagnostics_.error(loc,
                         "redefinition of function '" + *result.name + "'");
    declared.definition = DefinitionState::Defined;
    declared.parameters = result.params;
  }
  recordDeclaration(id, entity, file);
  return entity;
}

EntityId Sema::declareLocalObject(DeclId id, const DeclResult &result,
                                  SourceLoc loc) {
  if (model_.binding(current_, result.name).valid())
    diagnostics_.error(loc, "redefinition of '" + *result.name + "'");
  EntityId entity = model_.addEntity(EntityKind::Object, result.name,
                                     result.type, Linkage::None, loc);
  model_.entity(entity).definition = DefinitionState::Defined;
  model_.bind(current_, result.name, entity);
  recordDeclaration(id, entity, current_);
  return entity;
}

EntityId Sema::declareLocalFunction(DeclId id, const DeclResult &result,
                                    SourceLoc loc) {
  EntityId bound = model_.binding(current_, result.name);
  if (bound.valid()) {
    const Entity &existing = model_.entity(bound);
    if (existing.kind != EntityKind::Function)
      diagnostics_.error(loc, "redefinition of '" + *result.name +
                                  "' as a different kind of symbol");
    if (existing.type != result.type)
      diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
    recordDeclaration(id, bound, current_);
    return bound;
  }

  EntityId visible = lookupOrdinary(result.name);
  if (visible.valid()) {
    const Entity &existing = model_.entity(visible);
    if (existing.kind == EntityKind::Function) {
      if (existing.type != result.type)
        diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
      model_.bind(current_, result.name, visible);
      recordDeclaration(id, visible, current_);
      return visible;
    }
    // An object in an enclosing block is hidden by the function declaration;
    // the declaration denotes the external function instead.
  }

  auto external = externalEntities_.find(result.name);
  if (external != externalEntities_.end()) {
    EntityId entity = external->second;
    const Entity &existing = model_.entity(entity);
    if (existing.kind != EntityKind::Function)
      diagnostics_.error(loc, "redefinition of '" + *result.name +
                                  "' as a different kind of symbol");
    if (existing.type != result.type)
      diagnostics_.error(loc, "conflicting types for '" + *result.name + "'");
    model_.bind(current_, result.name, entity);
    recordDeclaration(id, entity, current_);
    return entity;
  }

  EntityId entity = model_.addEntity(EntityKind::Function, result.name,
                                     result.type, Linkage::External, loc);
  externalEntities_.emplace(result.name, entity);
  model_.bind(current_, result.name, entity);
  recordDeclaration(id, entity, current_);
  return entity;
}

const Type *Sema::resolveTypeSpec(TypeSpecId id, bool declareTag) {
  const TypeSpec &spec = ast_.typeSpec(id);
  if (std::holds_alternative<TypeSpec::Void>(spec.node))
    return types_.voidType();
  if (std::holds_alternative<TypeSpec::Char>(spec.node))
    return types_.charType();
  if (std::holds_alternative<TypeSpec::Int>(spec.node))
    return types_.intType();

  const TypeSpec::Struct &structSpec = std::get<TypeSpec::Struct>(spec.node);
  StructDecl *record = model_.typeSpec(id).record;
  if (record)
    return types_.structType(record);

  if (structSpec.hasBraces) {
    if (structSpec.tag) {
      record = model_.bindingTag(current_, structSpec.tag);
      if (record) {
        if (record->complete)
          diagnostics_.error(spec.loc,
                             "redefinition of 'struct " + *structSpec.tag +
                                 "'");
      } else {
        record = model_.addStruct(structSpec.tag, spec.loc, false);
        model_.bindTag(current_, structSpec.tag, record);
      }
    } else {
      record = model_.addStruct(Symbol(), spec.loc, true);
    }

    record->loc = spec.loc;
    record->anonymous = !structSpec.tag;
    record->type = types_.structType(record);
    record->fields.clear();
    model_.typeSpec(id).record = record;

    std::unordered_set<Symbol> fieldNames;
    for (DeclId fieldId : structSpec.fields) {
      const Decl &field = ast_.decl(fieldId);
      auto *fieldDecl = std::get_if<FieldDecl>(&field.node);
      if (!fieldDecl)
        diagnostics_.error(field.loc, "invalid struct member");

      const Type *base = resolveTypeSpec(fieldDecl->spec);
      DeclResult result = resolveDeclarator(fieldDecl->declarator, base);
      SourceLoc nameLoc = result.nameLoc.line ? result.nameLoc : field.loc;
      if (!result.name)
        diagnostics_.error(nameLoc, "struct member requires a name");
      if (!fieldNames.insert(result.name).second)
        diagnostics_.error(nameLoc,
                           "duplicate struct member '" + *result.name + "'");
      validateFieldType(result.type, result.name, nameLoc);

      FieldInfo info;
      info.name = result.name;
      info.type = result.type;
      info.decl = fieldId;
      record->fields.push_back(std::move(info));
    }

    if (record->fields.empty())
      diagnostics_.error(spec.loc, "struct has no members");

    record->complete = true;
    for (size_t i = 0; i < record->fields.size(); ++i)
      record->fields[i].offset = layout_.fieldOffset(record, unsigned(i));
    return record->type;
  }

  if (!structSpec.tag)
    diagnostics_.error(spec.loc, "expected struct tag or declaration");

  if (declareTag) {
    // A declaration of the form "struct S;" declares the tag in the current
    // scope even when an outer scope already declares a tag with that name.
    record = model_.bindingTag(current_, structSpec.tag);
    if (!record) {
      record = model_.addStruct(structSpec.tag, spec.loc, false);
      record->type = types_.structType(record);
      model_.bindTag(current_, structSpec.tag, record);
    }
  } else {
    record = lookupTag(structSpec.tag);
    if (!record) {
      record = model_.addStruct(structSpec.tag, spec.loc, false);
      record->type = types_.structType(record);
      model_.bindTag(current_, structSpec.tag, record);
    }
  }
  model_.typeSpec(id).record = record;
  return types_.structType(record);
}

Sema::DeclResult Sema::resolveDeclarator(DeclaratorId id, const Type *base) {
  DeclResult result;
  result.type = base;
  if (!id.valid())
    return result;

  const Declarator &declarator = ast_.declarator(id);
  if (auto *name = std::get_if<NameDeclarator>(&declarator.node)) {
    result.name = name->name;
    result.nameLoc = declarator.loc;
  } else if (auto *pointer = std::get_if<PointerDeclarator>(&declarator.node)) {
    result = resolveDeclarator(pointer->inner, types_.pointerTo(base));
  } else if (auto *function =
                 std::get_if<FunctionDeclarator>(&declarator.node)) {
    // C11 6.2.1p4/p7: names and tags declared in a parameter list have
    // function prototype scope, which ends with the function declarator.  The
    // scope is retained only when this declarator produces the declared
    // function type and the declaration is a definition (adoptFunctionScope).
    ScopeId prototypeScope = pushScope(ScopeKind::FunctionPrototype);
    std::vector<const Type *> parameterTypes;
    std::vector<DeclId> parameters = function->params;
    std::unordered_set<Symbol> parameterNames;

    for (DeclId paramId : parameters) {
      const Decl &param = ast_.decl(paramId);
      auto *paramDecl = std::get_if<ParamDecl>(&param.node);
      if (!paramDecl)
        diagnostics_.error(param.loc, "invalid parameter declaration");

      const Type *paramBase = resolveTypeSpec(paramDecl->spec);
      DeclResult paramResult =
          resolveDeclarator(paramDecl->declarator, paramBase);
      const Type *paramType =
          adjustParameterType(paramResult.type, param.loc);

      if (Type::isVoid(paramType))
        diagnostics_.error(param.loc, "parameter has incomplete type 'void'");
      if (Type::isFunction(paramType))
        diagnostics_.error(param.loc, "parameter has function type");
      SourceLoc paramLoc =
          paramResult.nameLoc.line ? paramResult.nameLoc : param.loc;
      if (paramResult.name) {
        if (!parameterNames.insert(paramResult.name).second)
          diagnostics_.error(paramLoc, "redefinition of parameter '" +
                                           *paramResult.name + "'");
        EntityId entity = model_.addEntity(
            EntityKind::Object, paramResult.name, paramType, Linkage::None,
            paramLoc);
        model_.entity(entity).definition = DefinitionState::Defined;
        model_.bind(prototypeScope, paramResult.name, entity);
        recordDeclaration(paramId, entity, prototypeScope);
      }
      parameterTypes.push_back(paramType);
    }

    const Type *functionType =
        types_.functionOf(base, std::move(parameterTypes));
    validateFunctionType(functionType, declarator.loc);
    DeclResult inner = resolveDeclarator(function->inner, functionType);
    if (inner.type == functionType) {
      inner.functionScope = prototypeScope;
      inner.params = std::move(parameters);
    }
    popScope();
    result = std::move(inner);
  }

  return result;
}

void Sema::adoptFunctionScope(ScopeId scope, ScopeId parent) {
  assert(scope.valid() && "function definition has no function scope");
  Scope &functionScope = model_.scope(scope);
  functionScope.kind = ScopeKind::Function;
  functionScope.parent = parent;
}

const Type *Sema::adjustParameterType(const Type *type, SourceLoc loc) {
  (void)loc;
  if (Type::isArray(type))
    return types_.pointerTo(type->element());
  if (Type::isFunction(type))
    return types_.pointerTo(type);
  return type;
}

bool Sema::isCompleteObject(const Type *type) const {
  if (!type || !Type::isObject(type))
    return false;
  if (Type::isStruct(type))
    return type->record() && type->record()->complete;
  if (Type::isArray(type))
    return isCompleteObject(type->element());
  return true;
}

void Sema::validateObjectType(const Type *type, SourceLoc loc, const char *what,
                              bool requireComplete) const {
  if (Type::isVoid(type))
    diagnostics_.error(loc,
                       std::string(what) + " has incomplete type 'void'");
  if (Type::isFunction(type))
    diagnostics_.error(loc, std::string(what) + " declared as a function");
  if (requireComplete && !isCompleteObject(type))
    diagnostics_.error(loc, std::string(what) + " has incomplete type '" +
                                 TypeContext::toString(type) + "'");
}

void Sema::validateFunctionType(const Type *type, SourceLoc loc) const {
  assert(Type::isFunction(type));
  const Type *result = type->result();
  if (Type::isFunction(result) || Type::isArray(result))
    diagnostics_.error(loc, "function cannot return a function or array type");
  if (Type::isStruct(result)) {
    if (!result->record() || !result->record()->complete)
      diagnostics_.error(loc, "function returns an incomplete struct type");
    diagnostics_.error(loc, "functions returning struct are not supported");
  }
  for (const Type *param : type->params()) {
    if (Type::isVoid(param))
      diagnostics_.error(loc, "parameter has incomplete type 'void'");
    if (Type::isFunction(param) || Type::isArray(param))
      diagnostics_.error(loc, "parameter has an invalid type");
    if (Type::isStruct(param)) {
      if (!param->record() || !param->record()->complete)
        diagnostics_.error(loc, "parameter has incomplete struct type");
      diagnostics_.error(loc, "struct parameters are not supported");
    }
  }
}

void Sema::validateFieldType(const Type *type, Symbol name,
                             SourceLoc loc) const {
  if (Type::isVoid(type))
    diagnostics_.error(loc, "member has incomplete type 'void'");
  if (Type::isFunction(type))
    diagnostics_.error(loc, "member declared as a function");
  if (!isCompleteObject(type)) {
    std::string message = "member";
    if (name)
      message += " '" + *name + "'";
    message += " has incomplete type '" + TypeContext::toString(type) + "'";
    diagnostics_.error(loc, message);
  }
}

void Sema::analyzeTopLevel(DeclId id) {
  const Decl &decl = ast_.decl(id);

  if (auto *var = std::get_if<VarDecl>(&decl.node)) {
    const Type *base = resolveTypeSpec(var->spec);
    DeclResult result = resolveDeclarator(var->declarator, base);
    SourceLoc nameLoc = result.nameLoc.line ? result.nameLoc : decl.loc;
    if (!result.name)
      diagnostics_.error(nameLoc, "declaration requires a name");
    // A file-scope object declaration without an initializer is a tentative
    // definition.  C11 permits an incomplete type here if the type is
    // completed later in the translation unit, so completeness is checked
    // after all top-level declarations have been analyzed.
    validateObjectType(result.type, nameLoc, "variable",
                       /*requireComplete=*/false);
    declareFileObject(id, result, nameLoc);
    return;
  }

  if (auto *function = std::get_if<FunctionDecl>(&decl.node)) {
    const Type *base = resolveTypeSpec(function->spec);
    bool isDefinition = function->body.valid();
    DeclResult result = resolveDeclarator(function->declarator, base);
    SourceLoc nameLoc = result.nameLoc.line ? result.nameLoc : decl.loc;
    if (!result.name)
      diagnostics_.error(nameLoc, "function declaration requires a name");
    if (!Type::isFunction(result.type))
      diagnostics_.error(nameLoc,
                         "function declaration does not declare a function");
    validateFunctionType(result.type, nameLoc);

    declareFileFunction(id, result, nameLoc, isDefinition);
    if (isDefinition)
      analyzeFunction(id, result.functionScope);
    return;
  }

  if (auto *tag = std::get_if<TagDecl>(&decl.node)) {
    const TypeSpec &spec = ast_.typeSpec(tag->spec);
    const auto *structSpec = std::get_if<TypeSpec::Struct>(&spec.node);
    if (structSpec && structSpec->hasBraces && !structSpec->tag) {
      diagnostics_.error(decl.loc, "declaration does not declare anything");
      return;
    }
    resolveTypeSpec(tag->spec, /*declareTag=*/true);
    DeclInfo &info = model_.decl(id);
    info.record = model_.typeSpec(tag->spec).record;
    info.scope = current_;
    return;
  }

  diagnostics_.error(decl.loc, "invalid top-level declaration");
}

void Sema::analyzeLocalDecl(DeclId id) {
  const Decl &decl = ast_.decl(id);

  if (auto *var = std::get_if<VarDecl>(&decl.node)) {
    const Type *base = resolveTypeSpec(var->spec);
    DeclResult result = resolveDeclarator(var->declarator, base);
    SourceLoc nameLoc = result.nameLoc.line ? result.nameLoc : decl.loc;
    if (!result.name)
      diagnostics_.error(nameLoc, "declaration requires a name");
    validateObjectType(result.type, nameLoc, "variable");
    declareLocalObject(id, result, nameLoc);
    return;
  }

  if (auto *function = std::get_if<FunctionDecl>(&decl.node)) {
    const Type *base = resolveTypeSpec(function->spec);
    DeclResult result = resolveDeclarator(function->declarator, base);
    SourceLoc nameLoc = result.nameLoc.line ? result.nameLoc : decl.loc;
    if (!result.name)
      diagnostics_.error(nameLoc, "function declaration requires a name");
    if (!Type::isFunction(result.type))
      diagnostics_.error(nameLoc,
                         "function declaration does not declare a function");
    validateFunctionType(result.type, nameLoc);
    declareLocalFunction(id, result, nameLoc);
    return;
  }

  if (auto *tag = std::get_if<TagDecl>(&decl.node)) {
    const TypeSpec &spec = ast_.typeSpec(tag->spec);
    const auto *structSpec = std::get_if<TypeSpec::Struct>(&spec.node);
    if (structSpec && structSpec->hasBraces && !structSpec->tag) {
      diagnostics_.error(decl.loc, "declaration does not declare anything");
      return;
    }
    resolveTypeSpec(tag->spec, /*declareTag=*/true);
    DeclInfo &info = model_.decl(id);
    info.record = model_.typeSpec(tag->spec).record;
    info.scope = current_;
    return;
  }

  diagnostics_.error(decl.loc, "invalid local declaration");
}

void Sema::analyzeFunction(DeclId id, ScopeId functionScope) {
  auto *function = std::get_if<FunctionDecl>(&ast_.decl(id).node);
  assert(function && function->body.valid());
  EntityId entityId = model_.decl(id).entity;
  const Entity &entity = model_.entity(entityId);
  const Type *functionType = entity.type;
  assert(Type::isFunction(functionType));

  for (DeclId paramId : entity.parameters) {
    if (!model_.decl(paramId).entity.valid())
      diagnostics_.error(declarationNameLoc(ast_, paramId),
                         "parameter name omitted in function definition");
  }

  // The prototype scope of the definition's function declarator becomes the
  // function scope: its parameters and parameter-list tags stay visible in the
  // body, and labels belong to it.  Its parent is the scope containing the
  // definition, not any enclosing declarator's prototype scope.
  adoptFunctionScope(functionScope, current_);
  ScopeId saved = current_;
  current_ = functionScope;

  FunctionContext context;
  context.returnType = functionType->result();
  context.scope = functionScope;
  function_ = &context;

  analyzeCompound(function->body, false);

  for (const auto &[gotoStmt, labelName] : context.pendingGotos) {
    LabelId target = model_.lookupLabel(context.scope, labelName);
    if (!target.valid())
      diagnostics_.error(ast_.stmt(gotoStmt).loc,
                         "use of undeclared label '" + *labelName + "'");
    model_.gotoTarget(gotoStmt) = target;
  }

  function_ = nullptr;
  current_ = saved;
}

void Sema::analyzeCompound(StmtId id, bool push) {
  auto *compound = std::get_if<CompoundStmt>(&ast_.stmt(id).node);
  assert(compound && "expected a compound statement");
  if (push)
    pushScope(ScopeKind::Block);
  for (StmtId item : compound->items) {
    if (auto *declStmt = std::get_if<DeclStmt>(&ast_.stmt(item).node))
      analyzeLocalDecl(declStmt->decl);
    else
      analyzeStmt(item);
  }
  if (push)
    popScope();
}

void Sema::analyzeStmt(StmtId id) {
  const Stmt &stmt = ast_.stmt(id);
  SourceLoc loc = stmt.loc;

  if (std::holds_alternative<CompoundStmt>(stmt.node)) {
    analyzeCompound(id, true);
    return;
  }

  if (auto *declStmt = std::get_if<DeclStmt>(&stmt.node)) {
    analyzeLocalDecl(declStmt->decl);
    return;
  }

  if (auto *exprStmt = std::get_if<ExprStmt>(&stmt.node)) {
    if (exprStmt->expr.valid())
      checkExpr(exprStmt->expr);
    return;
  }

  if (auto *ifStmt = std::get_if<IfStmt>(&stmt.node)) {
    checkScalar(ifStmt->cond, loc, "'if' condition");
    analyzeStmt(ifStmt->thenStmt);
    if (ifStmt->elseStmt.valid())
      analyzeStmt(ifStmt->elseStmt);
    return;
  }

  if (auto *whileStmt = std::get_if<WhileStmt>(&stmt.node)) {
    checkScalar(whileStmt->cond, loc, "'while' condition");
    assert(function_ && "loop outside of a function");
    ++function_->loopDepth;
    analyzeStmt(whileStmt->body);
    --function_->loopDepth;
    return;
  }

  if (auto *returnStmt = std::get_if<ReturnStmt>(&stmt.node)) {
    assert(function_ && "return outside of a function");
    const Type *returnType = function_->returnType;
    if (returnStmt->expr.valid()) {
      checkExpr(returnStmt->expr);
      if (Type::isVoid(returnType))
        diagnostics_.error(loc,
                           "return with a value in a void function");
      if (!assignable(returnType, returnStmt->expr))
        diagnostics_.error(
            loc, "returning '" +
                     TypeContext::toString(decayedType(
                         model_.expr(returnStmt->expr))) +
                     "' from a function with return type '" +
                     TypeContext::toString(returnType) + "'");
    } else if (!Type::isVoid(returnType)) {
      diagnostics_.error(loc,
                         "return without a value in a non-void function");
    }
    return;
  }

  if (std::holds_alternative<BreakStmt>(stmt.node)) {
    if (!function_ || function_->loopDepth == 0)
      diagnostics_.error(loc, "'break' outside of a loop");
    return;
  }

  if (std::holds_alternative<ContinueStmt>(stmt.node)) {
    if (!function_ || function_->loopDepth == 0)
      diagnostics_.error(loc, "'continue' outside of a loop");
    return;
  }

  if (auto *gotoStmt = std::get_if<GotoStmt>(&stmt.node)) {
    assert(function_ && "goto outside of a function");
    function_->pendingGotos.emplace_back(id, gotoStmt->name);
    return;
  }

  if (auto *labelStmt = std::get_if<LabelStmt>(&stmt.node)) {
    assert(function_ && "label outside of a function");
    const LabelDecl &label = ast_.label(labelStmt->label);
    if (model_.lookupLabel(function_->scope, label.name).valid())
      diagnostics_.error(label.loc,
                         "redefinition of label '" + *label.name + "'");
    model_.bindLabel(function_->scope, label.name, labelStmt->label);
    model_.label(labelStmt->label).defined = true;
    model_.label(labelStmt->label).loc = label.loc;
    analyzeStmt(labelStmt->stmt);
    return;
  }

  diagnostics_.error(loc, "invalid statement");
}

const Type *Sema::decayedType(const ExprInfo &info) {
  const Type *type = info.type;
  if (!type)
    return nullptr;
  if (Type::isArray(type))
    return types_.pointerTo(type->element());
  if (Type::isFunction(type))
    return types_.pointerTo(type);
  return type;
}

void Sema::setIntConstant(ExprInfo &info, long long value) const {
  info.type = types_.intType();
  info.category = ValueCategory::RValue;
  info.isConstant = true;
  info.value = wrapInt(value);
  info.nullPointerConstant = info.value == 0;
}

bool Sema::compatibleObjectPointers(const Type *a, const Type *b) const {
  if (!Type::isPointer(a) || !Type::isPointer(b))
    return false;
  if (a->pointee() != b->pointee())
    return false;
  return Type::isObject(a->pointee()) && Type::isObject(b->pointee());
}

bool Sema::compatibleEqualityPointers(const Type *a, const Type *b) const {
  if (!Type::isPointer(a) || !Type::isPointer(b))
    return false;
  const Type *aPointee = a->pointee();
  const Type *bPointee = b->pointee();
  if (aPointee == bPointee)
    return true;
  bool aVoid = Type::isVoid(aPointee);
  bool bVoid = Type::isVoid(bPointee);
  bool aObject = Type::isObject(aPointee);
  bool bObject = Type::isObject(bPointee);
  if (aVoid && bObject)
    return true;
  if (bVoid && aObject)
    return true;
  return false;
}

void Sema::checkPointerArithmetic(const Type *pointer, SourceLoc loc) const {
  if (!Type::isPointer(pointer))
    diagnostics_.error(loc, "pointer arithmetic requires a pointer operand");
  const Type *element = pointer->pointee();
  if (Type::isVoid(element))
    diagnostics_.error(loc, "arithmetic on a pointer to void");
  if (Type::isFunction(element))
    diagnostics_.error(loc, "arithmetic on a pointer to function");
  if (!isCompleteObject(element))
    diagnostics_.error(loc, "arithmetic on a pointer to incomplete type '" +
                                TypeContext::toString(element) + "'");
}

bool Sema::assignable(const Type *to, ExprId from) {
  const ExprInfo &fromInfo = model_.expr(from);
  const Type *fromType = decayedType(fromInfo);

  if (Type::isInteger(to))
    return Type::isInteger(fromType);

  if (Type::isPointer(to)) {
    if (fromInfo.nullPointerConstant)
      return true;
    if (!Type::isPointer(fromType))
      return false;
    if (to->pointee() == fromType->pointee())
      return true;
    bool toVoid = Type::isVoid(to->pointee());
    bool fromVoid = Type::isVoid(fromType->pointee());
    bool toObject = Type::isObject(to->pointee());
    bool fromObject = Type::isObject(fromType->pointee());
    if (toVoid && fromObject)
      return true;
    if (fromVoid && toObject)
      return true;
    return false;
  }

  if (Type::isStruct(to))
    return fromType == to;

  return false;
}

void Sema::checkScalar(ExprId id, SourceLoc loc, const char *what) {
  const ExprInfo &info = checkExpr(id);
  const Type *type = decayedType(info);
  if (!Type::isScalar(type)) {
    if (Type::isVoid(type))
      diagnostics_.error(loc, std::string(what) +
                                  " requires a scalar expression, but found "
                                  "void");
    diagnostics_.error(loc, std::string(what) +
                                " requires a scalar expression, but found '" +
                                TypeContext::toString(type) + "'");
  }
}

void Sema::checkSizeofType(const Type *type, SourceLoc loc,
                           ExprInfo &info) const {
  if (!isCompleteObject(type)) {
    if (Type::isVoid(type))
      diagnostics_.error(loc, "invalid application of 'sizeof' to void");
    if (Type::isFunction(type))
      diagnostics_.error(loc,
                         "invalid application of 'sizeof' to function type");
    diagnostics_.error(loc,
                       "invalid application of 'sizeof' to incomplete type '" +
                           TypeContext::toString(type) + "'");
  }
  setIntConstant(info, static_cast<long long>(layout_.sizeOf(type)));
}

ExprInfo &Sema::checkExpr(ExprId id) {
  ExprInfo &info = model_.expr(id);
  if (info.type)
    return info;

  const Expr &expr = ast_.expr(id);
  SourceLoc loc = expr.loc;

  if (auto *identifier = std::get_if<IdentifierExpr>(&expr.node)) {
    EntityId entity = lookupOrdinary(identifier->name);
    if (!entity.valid())
      diagnostics_.error(loc, "use of undeclared identifier '" +
                                  *identifier->name + "'");
    const Entity &resolved = model_.entity(entity);
    if (!resolved.type)
      diagnostics_.error(loc, "internal error: unresolved declaration '" +
                                  *identifier->name + "'");
    info.type = resolved.type;
    info.entity = entity;
    info.category = resolved.kind == EntityKind::Function
                        ? ValueCategory::FunctionDesignator
                        : ValueCategory::LValue;
    return info;
  }

  if (auto *constant = std::get_if<ConstantExpr>(&expr.node)) {
    setIntConstant(info,
                   decodeIntegerConstant(constant->spelling, loc,
                                         diagnostics_));
    return info;
  }

  if (auto *string = std::get_if<StringLiteralExpr>(&expr.node)) {
    std::vector<char> bytes =
        decodeStringLiteral(string->spelling, loc, diagnostics_);
    info.type = types_.arrayOf(types_.charType(),
                               static_cast<uint64_t>(bytes.size()) + 1);
    info.category = ValueCategory::LValue;
    info.stringBytes = std::move(bytes);
    return info;
  }

  if (auto *unary = std::get_if<UnaryExpr>(&expr.node)) {
    ExprInfo &operand = checkExpr(unary->operand);
    const Type *type = decayedType(operand);
    switch (unary->op) {
    case UnaryExpr::Op::Plus:
      if (!Type::isInteger(type))
        diagnostics_.error(loc, "invalid operand to unary '+'");
      if (operand.isConstant)
        setIntConstant(info, operand.value);
      else {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      }
      break;
    case UnaryExpr::Op::Minus:
      if (!Type::isInteger(type))
        diagnostics_.error(loc, "invalid operand to unary '-'");
      if (operand.isConstant)
        setIntConstant(info, -operand.value);
      else {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      }
      break;
    case UnaryExpr::Op::Not:
      if (!Type::isScalar(type))
        diagnostics_.error(loc, "invalid operand to unary '!'");
      if (operand.isConstant)
        setIntConstant(info, operand.value == 0 ? 1 : 0);
      else {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      }
      break;
    case UnaryExpr::Op::Deref:
      if (!Type::isPointer(type))
        diagnostics_.error(loc, "indirection requires a pointer operand");
      if (Type::isVoid(type->pointee()))
        diagnostics_.error(loc, "cannot dereference a pointer to void");
      info.type = type->pointee();
      info.category = Type::isFunction(type->pointee())
                          ? ValueCategory::FunctionDesignator
                          : ValueCategory::LValue;
      break;
    case UnaryExpr::Op::Addr:
      if (operand.category != ValueCategory::LValue &&
          operand.category != ValueCategory::FunctionDesignator)
        diagnostics_.error(loc, "cannot take the address of an rvalue");
      info.type = types_.pointerTo(operand.type);
      info.category = ValueCategory::RValue;
      break;
    }
    return info;
  }

  if (auto *binary = std::get_if<BinaryExpr>(&expr.node)) {
    ExprInfo &left = checkExpr(binary->lhs);
    ExprInfo &right = checkExpr(binary->rhs);
    const Type *leftType = decayedType(left);
    const Type *rightType = decayedType(right);

    switch (binary->op) {
    case BinaryExpr::Op::Mul:
      if (!Type::isInteger(leftType) || !Type::isInteger(rightType))
        diagnostics_.error(loc, "invalid operands to binary '*'");
      if (left.isConstant && right.isConstant)
        setIntConstant(info, left.value * right.value);
      else {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      }
      break;
    case BinaryExpr::Op::Add:
      if (Type::isInteger(leftType) && Type::isInteger(rightType)) {
        if (left.isConstant && right.isConstant)
          setIntConstant(info, left.value + right.value);
        else {
          info.type = types_.intType();
          info.category = ValueCategory::RValue;
        }
      } else if (Type::isPointer(leftType) &&
                 Type::isInteger(rightType)) {
        checkPointerArithmetic(leftType, loc);
        info.type = leftType;
        info.category = ValueCategory::RValue;
      } else if (Type::isInteger(leftType) &&
                 Type::isPointer(rightType)) {
        checkPointerArithmetic(rightType, loc);
        info.type = rightType;
        info.category = ValueCategory::RValue;
      } else {
        diagnostics_.error(loc, "invalid operands to binary '+'");
      }
      break;
    case BinaryExpr::Op::Sub:
      if (Type::isInteger(leftType) && Type::isInteger(rightType)) {
        if (left.isConstant && right.isConstant)
          setIntConstant(info, left.value - right.value);
        else {
          info.type = types_.intType();
          info.category = ValueCategory::RValue;
        }
      } else if (Type::isPointer(leftType) &&
                 Type::isInteger(rightType)) {
        checkPointerArithmetic(leftType, loc);
        info.type = leftType;
        info.category = ValueCategory::RValue;
      } else if (Type::isPointer(leftType) &&
                 Type::isPointer(rightType)) {
        if (!compatibleObjectPointers(leftType, rightType))
          diagnostics_.error(loc, "invalid operands to binary '-'");
        if (!isCompleteObject(leftType->pointee()) ||
            !isCompleteObject(rightType->pointee()))
          diagnostics_.error(
              loc, "subtraction of pointers to incomplete type");
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      } else {
        diagnostics_.error(loc, "invalid operands to binary '-'");
      }
      break;
    case BinaryExpr::Op::Less:
      if (Type::isInteger(leftType) && Type::isInteger(rightType)) {
        if (left.isConstant && right.isConstant)
          setIntConstant(info, left.value < right.value ? 1 : 0);
        else {
          info.type = types_.intType();
          info.category = ValueCategory::RValue;
        }
      } else if (Type::isPointer(leftType) &&
                 Type::isPointer(rightType) &&
                 compatibleObjectPointers(leftType, rightType)) {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      } else {
        diagnostics_.error(loc, "invalid operands to binary '<'");
      }
      break;
    case BinaryExpr::Op::Equal:
    case BinaryExpr::Op::NotEqual: {
      bool equal = binary->op == BinaryExpr::Op::Equal;
      if (Type::isInteger(leftType) && Type::isInteger(rightType)) {
        if (left.isConstant && right.isConstant)
          setIntConstant(info, (left.value == right.value) == equal ? 1 : 0);
        else {
          info.type = types_.intType();
          info.category = ValueCategory::RValue;
        }
      } else if (Type::isPointer(leftType) &&
                 Type::isPointer(rightType) &&
                 compatibleEqualityPointers(leftType, rightType)) {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      } else if (Type::isPointer(leftType) &&
                 right.nullPointerConstant) {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      } else if (left.nullPointerConstant &&
                 Type::isPointer(rightType)) {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      } else {
        diagnostics_.error(loc, "invalid operands to equality operator");
      }
      break;
    }
    case BinaryExpr::Op::LogicalAnd:
    case BinaryExpr::Op::LogicalOr: {
      bool isAnd = binary->op == BinaryExpr::Op::LogicalAnd;
      if (!Type::isScalar(leftType) || !Type::isScalar(rightType))
        diagnostics_.error(loc, "invalid operands to logical operator");
      if (left.isConstant && right.isConstant) {
        bool l = left.value != 0;
        bool r = right.value != 0;
        setIntConstant(info, isAnd ? (l && r) : (l || r));
      } else if (left.isConstant) {
        bool l = left.value != 0;
        if (isAnd && !l)
          setIntConstant(info, 0);
        else if (!isAnd && l)
          setIntConstant(info, 1);
        else {
          info.type = types_.intType();
          info.category = ValueCategory::RValue;
        }
      } else {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      }
      break;
    }
    }
    return info;
  }

  if (auto *assign = std::get_if<AssignExpr>(&expr.node)) {
    ExprInfo &left = checkExpr(assign->lhs);
    checkExpr(assign->rhs);
    if (left.category != ValueCategory::LValue)
      diagnostics_.error(loc, "expression is not assignable");
    const Type *leftType = left.type;
    if (!Type::isObject(leftType))
      diagnostics_.error(loc, "expression is not assignable");
    if (Type::isArray(leftType))
      diagnostics_.error(loc, "array type is not assignable");
    if (Type::isStruct(leftType))
      diagnostics_.error(loc,
                         "assignment of struct type is not supported");
    if (!isCompleteObject(leftType))
      diagnostics_.error(loc, "assignment to incomplete type '" +
                                  TypeContext::toString(leftType) + "'");
    if (!assignable(leftType, assign->rhs))
      diagnostics_.error(
          loc, "assigning to '" + TypeContext::toString(leftType) +
                   "' from incompatible type '" +
                   TypeContext::toString(
                       decayedType(model_.expr(assign->rhs))) +
                   "'");
    info.type = leftType;
    info.category = ValueCategory::RValue;
    return info;
  }

  if (auto *conditional = std::get_if<ConditionalExpr>(&expr.node)) {
    checkScalar(conditional->cond, loc, "conditional operator");
    ExprInfo &thenInfo = checkExpr(conditional->thenExpr);
    ExprInfo &elseInfo = checkExpr(conditional->elseExpr);
    const Type *thenType = decayedType(thenInfo);
    const Type *elseType = decayedType(elseInfo);

    if (Type::isInteger(thenType) && Type::isInteger(elseType)) {
      const ExprInfo &condInfo = model_.expr(conditional->cond);
      if (condInfo.isConstant && thenInfo.isConstant &&
          elseInfo.isConstant)
        setIntConstant(info, condInfo.value != 0 ? thenInfo.value
                                                 : elseInfo.value);
      else {
        info.type = types_.intType();
        info.category = ValueCategory::RValue;
      }
    } else if (Type::isVoid(thenType) && Type::isVoid(elseType)) {
      info.type = types_.voidType();
      info.category = ValueCategory::Void;
    } else if (Type::isPointer(thenType) && Type::isPointer(elseType)) {
      if (!compatibleEqualityPointers(thenType, elseType))
        diagnostics_.error(
            loc, "incompatible operand types in conditional expression");
      if (Type::isVoid(thenType->pointee()))
        info.type = thenType;
      else if (Type::isVoid(elseType->pointee()))
        info.type = elseType;
      else
        info.type = thenType;
      info.category = ValueCategory::RValue;
    } else if (Type::isPointer(thenType) && elseInfo.nullPointerConstant) {
      info.type = thenType;
      info.category = ValueCategory::RValue;
    } else if (thenInfo.nullPointerConstant &&
               Type::isPointer(elseType)) {
      info.type = elseType;
      info.category = ValueCategory::RValue;
    } else {
      diagnostics_.error(
          loc, "incompatible operand types in conditional expression");
    }
    return info;
  }

  if (auto *call = std::get_if<CallExpr>(&expr.node)) {
    ExprInfo &callee = checkExpr(call->callee);
    const Type *calleeType = decayedType(callee);
    const Type *functionType = nullptr;
    if (Type::isFunction(calleeType))
      functionType = calleeType;
    else if (Type::isPointer(calleeType) &&
             Type::isFunction(calleeType->pointee()))
      functionType = calleeType->pointee();
    else
      diagnostics_.error(
          loc, "called object is not a function or function pointer");

    if (call->args.size() != functionType->params().size())
      diagnostics_.error(
          loc, "function call has " + std::to_string(call->args.size()) +
                   " arguments but " +
                   std::to_string(functionType->params().size()) +
                   " are required");

    for (size_t i = 0; i < call->args.size(); ++i) {
      checkExpr(call->args[i]);
      if (!assignable(functionType->params()[i], call->args[i]))
        diagnostics_.error(
            ast_.expr(call->args[i]).loc,
            "argument " + std::to_string(i + 1) + " has incompatible type '" +
                TypeContext::toString(
                    decayedType(model_.expr(call->args[i]))) +
                "'");
    }

    info.type = functionType->result();
    info.category = Type::isVoid(functionType->result())
                        ? ValueCategory::Void
                        : ValueCategory::RValue;
    return info;
  }

  if (auto *subscript = std::get_if<SubscriptExpr>(&expr.node)) {
    ExprInfo &base = checkExpr(subscript->base);
    ExprInfo &index = checkExpr(subscript->index);
    const Type *baseType = decayedType(base);
    const Type *indexType = decayedType(index);
    const Type *pointerType = nullptr;
    if (Type::isPointer(baseType) && Type::isInteger(indexType))
      pointerType = baseType;
    else if (Type::isInteger(baseType) && Type::isPointer(indexType))
      pointerType = indexType;
    else
      diagnostics_.error(
          loc, "subscripted value is not a pointer or array");
    checkPointerArithmetic(pointerType, loc);
    info.type = pointerType->pointee();
    info.category = ValueCategory::LValue;
    return info;
  }

  if (auto *member = std::get_if<MemberExpr>(&expr.node)) {
    ExprInfo &base = checkExpr(member->base);
    StructDecl *record = nullptr;
    ValueCategory category = ValueCategory::RValue;
    if (member->arrow) {
      const Type *baseType = decayedType(base);
      if (!Type::isPointer(baseType) ||
          !Type::isStruct(baseType->pointee()))
        diagnostics_.error(
            loc, "member reference type is not a pointer to struct");
      record = baseType->pointee()->record();
      category = ValueCategory::LValue;
    } else {
      if (!Type::isStruct(base.type))
        diagnostics_.error(loc,
                           "member reference base type is not a struct");
      record = base.type->record();
      category = base.category == ValueCategory::LValue
                     ? ValueCategory::LValue
                     : ValueCategory::RValue;
    }

    if (!record || !record->complete)
      diagnostics_.error(loc,
                         "member reference to incomplete struct type");

    for (size_t i = 0; i < record->fields.size(); ++i) {
      if (record->fields[i].name == member->member) {
        info.type = record->fields[i].type;
        info.category = category;
        info.memberRecord = record;
        info.memberIndex = static_cast<unsigned>(i);
        return info;
      }
    }

    diagnostics_.error(loc, "no member named '" + *member->member +
                                "' in '" +
                                TypeContext::toString(base.type) + "'");
  }

  if (auto *sizeOf = std::get_if<SizeofExpr>(&expr.node)) {
    if (auto *typeName =
            std::get_if<SizeofExpr::TypeName>(&sizeOf->target)) {
      const Type *base = resolveTypeSpec(typeName->spec);
      DeclResult result = resolveDeclarator(typeName->declarator, base);
      checkSizeofType(result.type, loc, info);
    } else {
      ExprInfo &operand = checkExpr(std::get<ExprId>(sizeOf->target));
      checkSizeofType(operand.type, loc, info);
    }
    return info;
  }

  diagnostics_.error(loc, "invalid expression");
}

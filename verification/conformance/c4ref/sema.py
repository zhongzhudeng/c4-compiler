"""Independent executable Sema checker for the C4 subset.

Derived from verification/spec/sema.md (Assignment 3, Task E) and the C11
Constraints/Semantics clauses it incorporates.  This checker is deliberately
independent of src/Sema.cpp: it uses its own type representation, scopes, and
name-resolution rules.  It is an executable reference, not a proof.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from .ast import Decl, Declarator, Expr, Loc, Program, Stmt, TypeSpec


# ---------------------------------------------------------------------------
# Types
# ---------------------------------------------------------------------------

@dataclass
class StructInfo:
    id: int
    tag: Optional[str]
    loc: Loc
    scope_depth: int
    complete: bool = False
    fields: List["Field"] = field(default_factory=list)


@dataclass
class Field:
    name: str
    type: "Type"
    loc: Loc


@dataclass(frozen=True)
class Type:
    kind: str                       # void, char, int, pointer, function, struct, array
    elem: Optional["Type"] = None
    result: Optional["Type"] = None
    params: Tuple["Type", ...] = ()
    struct: Optional[StructInfo] = None
    bound: Optional[int] = None


T_VOID = Type("void")
T_CHAR = Type("char")
T_INT = Type("int")


def ptr(t: Type) -> Type:
    return Type("pointer", elem=t)


def func(result: Type, params: Tuple[Type, ...]) -> Type:
    return Type("function", result=result, params=params)


def array(t: Type, bound: int) -> Type:
    return Type("array", elem=t, bound=bound)


def struct_type(info: StructInfo) -> Type:
    return Type("struct", struct=info)


def type_eq(a: Type, b: Type) -> bool:
    if a.kind != b.kind:
        return False
    if a.kind in ("void", "char", "int"):
        return True
    if a.kind == "pointer":
        return type_eq(a.elem, b.elem)
    if a.kind == "array":
        return a.bound == b.bound and type_eq(a.elem, b.elem)
    if a.kind == "function":
        return (type_eq(a.result, b.result) and len(a.params) == len(b.params)
                and all(type_eq(x, y) for x, y in zip(a.params, b.params)))
    if a.kind == "struct":
        return a.struct is b.struct
    return False


def type_str(t: Type) -> str:
    if t.kind in ("void", "char", "int"):
        return t.kind
    if t.kind == "pointer":
        return "%s *" % type_str(t.elem)
    if t.kind == "array":
        return "%s[%d]" % (type_str(t.elem), t.bound)
    if t.kind == "function":
        return "%s (%s)" % (type_str(t.result),
                            ", ".join(type_str(p) for p in t.params) or "void")
    if t.kind == "struct":
        return "struct %s" % (t.struct.tag or "<anonymous>")
    return t.kind


def is_integer(t: Type) -> bool:
    return t.kind in ("char", "int")


def is_arithmetic(t: Type) -> bool:
    return is_integer(t)


def is_scalar(t: Type) -> bool:
    return is_arithmetic(t) or t.kind == "pointer"


def is_object(t: Type) -> bool:
    return t.kind not in ("function", "void")


def is_complete(t: Type) -> bool:
    if t.kind in ("void", "function"):
        return False
    if t.kind == "struct":
        return t.struct.complete
    if t.kind == "array":
        return is_complete(t.elem) and t.bound is not None
    return True


# ---------------------------------------------------------------------------
# Symbols and scopes
# ---------------------------------------------------------------------------

@dataclass
class Symbol:
    name: str
    type: Type
    kind: str                       # "object" | "function"
    loc: Loc
    is_global: bool
    is_definition: bool = False
    is_tentative: bool = False


class Scope:
    def __init__(self, parent: Optional["Scope"], depth: int):
        self.parent = parent
        self.depth = depth
        self.ordinary: Dict[str, Symbol] = {}
        self.tags: Dict[str, StructInfo] = {}

    def lookup_ordinary(self, name: str) -> Optional[Symbol]:
        scope: Optional[Scope] = self
        while scope is not None:
            if name in scope.ordinary:
                return scope.ordinary[name]
            scope = scope.parent
        return None

    def lookup_tag(self, name: str) -> Optional[StructInfo]:
        scope: Optional[Scope] = self
        while scope is not None:
            if name in scope.tags:
                return scope.tags[name]
            scope = scope.parent
        return None


class SemaError(Exception):
    def __init__(self, loc: Loc, message: str, category: str = "invalid"):
        super().__init__(message)
        self.loc = loc
        self.message = message
        self.category = category


class SemaExempted(SemaError):
    def __init__(self, loc: Loc, message: str):
        super().__init__(loc, message, category="exempted")


@dataclass
class ExprInfo:
    type: Type
    category: str = "rvalue"        # lvalue | rvalue | function | void
    is_null: bool = False
    is_const: bool = False
    value: int = 0
    symbol: Optional[Symbol] = None
    field: Optional[Field] = None
    label: Optional[str] = None


@dataclass
class FunctionContext:
    return_type: Type
    loop_depth: int = 0
    labels: Dict[str, Loc] = field(default_factory=dict)
    pending_gotos: List[Tuple[str, Loc]] = field(default_factory=list)


@dataclass
class SemaResult:
    accepted: bool
    exempted: bool
    error: Optional[SemaError]
    note: str = ""


# ---------------------------------------------------------------------------
# Checker
# ---------------------------------------------------------------------------

class Sema:
    def __init__(self, program: Program):
        self.program = program
        self.scopes: List[Scope] = []
        self.current: Optional[Scope] = None
        self.global_functions: Dict[str, Symbol] = {}
        self.tentative: List[Tuple[Symbol, Loc]] = []
        self.function: Optional[FunctionContext] = None
        self.struct_counter = 0

    # -- scope helpers -------------------------------------------------
    def push_scope(self):
        self.scopes.append(Scope(self.current, len(self.scopes)))
        self.current = self.scopes[-1]

    def pop_scope(self):
        self.scopes.pop()
        self.current = self.scopes[-1] if self.scopes else None

    def at_file_scope(self) -> bool:
        return self.current is not None and self.current.depth == 0

    # -- diagnostics ---------------------------------------------------
    @staticmethod
    def error(loc: Loc, message: str):
        raise SemaError(loc, message)

    @staticmethod
    def exempt(loc: Loc, message: str):
        raise SemaExempted(loc, message)

    # -- top level -----------------------------------------------------
    def analyze(self) -> SemaResult:
        try:
            self.push_scope()
            for decl in self.program.decls:
                self.analyze_external(decl)
            for symbol, loc in self.tentative:
                if not is_complete(symbol.type):
                    self.error(loc, "tentative definition of '%s' has incomplete type"
                               % symbol.name)
            self.pop_scope()
            return SemaResult(True, False, None)
        except SemaExempted as exc:
            return SemaResult(False, True, exc)
        except SemaError as exc:
            return SemaResult(False, False, exc)

    def analyze_external(self, decl: Decl):
        if decl.body is not None:
            self.analyze_function_definition(decl)
        else:
            self.analyze_declaration(decl, is_global=True)

    # -- structs -------------------------------------------------------
    def new_struct(self, tag: Optional[str], loc: Loc, scope: Scope) -> StructInfo:
        self.struct_counter += 1
        return StructInfo(self.struct_counter, tag, loc, scope.depth)

    def declare_tag_forward(self, spec: TypeSpec):
        """C11 6.7.2.3p7: a declaration of the form 'struct S;' declares
        the identifier as a tag in the current scope.  Unlike a mere use
        such as 'struct S *p;', it never reuses a tag from an enclosing
        scope, so it shadows that outer tag."""
        assert spec.tag is not None
        if spec.tag in self.current.tags:
            return
        self.current.tags[spec.tag] = self.new_struct(
            spec.tag, spec.loc, self.current)

    def resolve_type_spec(self, spec: TypeSpec) -> Type:
        if spec.kind in ("void", "char", "int"):
            return Type(spec.kind)
        if spec.kind != "struct":
            self.error(spec.loc, "unknown type specifier")
        if spec.has_braces:
            info: Optional[StructInfo] = None
            if spec.tag is not None:
                info = self.current.tags.get(spec.tag)
                if info is not None:
                    if info.complete:
                        self.error(spec.loc, "redefinition of struct '%s'" % spec.tag)
                else:
                    info = self.new_struct(spec.tag, spec.loc, self.current)
                    self.current.tags[spec.tag] = info
            else:
                info = self.new_struct(None, spec.loc, self.current)
            # The tag is visible inside its own definition; keep info
            # incomplete while resolving fields.
            fields: List[Field] = []
            named = 0
            for field_decl in spec.fields:
                field_spec_type = self.resolve_type_spec(field_decl.spec)
                if field_decl.declarator is None:
                    self.exempt(field_decl.loc,
                                "anonymous struct member is exempted by Task E")
                ftype = self.resolve_declarator(field_decl.declarator,
                                                field_spec_type)
                if not is_complete(ftype):
                    self.error(field_decl.loc,
                               "member '%s' has incomplete type"
                               % (field_decl.declarator.name or "<anonymous>"))
                if ftype.kind == "function":
                    self.error(field_decl.loc, "member cannot have function type")
                name = field_decl.declarator.name
                assert name is not None
                for existing in fields:
                    if existing.name == name:
                        self.error(field_decl.loc,
                                   "duplicate member '%s'" % name)
                fields.append(Field(name, ftype, field_decl.loc))
                named += 1
            if named == 0:
                self.error(spec.loc, "struct has no named members")
            info.fields = fields
            info.complete = True
            return struct_type(info)
        # A reference or forward declaration.
        assert spec.tag is not None
        info = self.current.lookup_tag(spec.tag)
        if info is None:
            info = self.new_struct(spec.tag, spec.loc, self.current)
            self.current.tags[spec.tag] = info
        return struct_type(info)

    # -- declarators ---------------------------------------------------
    def resolve_declarator(self, declarator: Declarator, base: Type) -> Type:
        t = base
        for ctor in reversed(declarator.ctors):
            if ctor.kind == "pointer":
                t = ptr(t)
            elif ctor.kind == "function":
                t = self.make_function(t, ctor.params)
            else:
                self.error(declarator.loc, "unknown declarator constructor")
        return t

    def make_function(self, result: Type, params: List[Decl]) -> Type:
        if result.kind in ("function", "array"):
            self.error(params[0].loc if params else Loc(1, 1),
                       "function cannot return %s type" % result.kind)
        # The special (void) parameter list.
        if len(params) == 1 and params[0].declarator is None:
            ptype = self.resolve_type_spec(params[0].spec)
            if ptype.kind == "void":
                return func(result, ())
        adjusted: List[Type] = []
        for p in params:
            ptype = self.resolve_type_spec(p.spec)
            if p.declarator is not None:
                ptype = self.resolve_declarator(p.declarator, ptype)
            if ptype.kind == "void":
                self.error(p.loc, "void parameter must be the only parameter and unnamed")
            if ptype.kind == "function":
                ptype = ptr(ptype)
            if ptype.kind == "array":
                ptype = ptr(ptype.elem)
            adjusted.append(ptype)
        return func(result, tuple(adjusted))

    # -- declarations --------------------------------------------------
    def analyze_declaration(self, decl: Decl, is_global: bool):
        base = self.resolve_type_spec(decl.spec)
        if decl.declarator is None:
            if decl.spec.kind == "struct" and decl.spec.has_braces:
                return
            if decl.spec.kind == "struct" and decl.spec.tag:
                self.declare_tag_forward(decl.spec)
                return
            self.error(decl.loc, "declaration does not declare anything")
        declarator = decl.declarator
        assert declarator is not None
        name = declarator.name
        if name is None:
            self.error(decl.loc, "declaration has no identifier")
        t = self.resolve_declarator(declarator, base)
        if t.kind == "function":
            self.declare_function(name, t, declarator.loc, is_global=is_global,
                                  is_definition=False)
        else:
            self.declare_object(name, t, declarator.loc, is_global=is_global)

    def declare_function(self, name: str, t: Type, loc: Loc,
                         is_global: bool, is_definition: bool):
        # Task E exemption: by-value struct parameter or return.
        if t.result.kind == "struct":
            self.exempt(loc, "function returning struct is exempted by Task E")
        for p in t.params:
            if p.kind == "struct":
                self.exempt(loc, "function with struct parameter is exempted by Task E")
        if is_global or self.at_file_scope():
            existing = self.global_functions.get(name)
            if existing is not None:
                if not type_eq(existing.type, t):
                    self.error(loc, "incompatible redeclaration of function '%s'" % name)
                if is_definition:
                    if existing.is_definition:
                        self.error(loc, "redefinition of function '%s'" % name)
                    existing.is_definition = True
                self.current.ordinary[name] = existing
                return
            prior = self.current.ordinary.get(name)
            if prior is not None:
                if prior.kind != "function" or not type_eq(prior.type, t):
                    self.error(loc, "incompatible redeclaration of '%s'" % name)
                if is_definition:
                    if prior.is_definition:
                        self.error(loc, "redefinition of function '%s'" % name)
                    prior.is_definition = True
                self.global_functions[name] = prior
                return
            symbol = Symbol(name, t, "function", loc, True,
                            is_definition=is_definition)
            self.current.ordinary[name] = symbol
            self.global_functions[name] = symbol
            return

        # Block scope: a function declaration has external linkage and denotes
        # the same entity as a file-scope function.
        local = self.current.ordinary.get(name)
        if local is not None:
            if local.kind != "function" or not type_eq(local.type, t):
                self.error(loc, "incompatible redeclaration of '%s'" % name)
            return
        symbol = self.global_functions.get(name)
        if symbol is None:
            symbol = Symbol(name, t, "function", loc, True,
                            is_definition=False)
            self.global_functions[name] = symbol
        elif not type_eq(symbol.type, t):
            self.error(loc, "incompatible redeclaration of function '%s'" % name)
        self.current.ordinary[name] = symbol

    def declare_object(self, name: str, t: Type, loc: Loc, is_global: bool):
        if t.kind == "void":
            self.error(loc, "object '%s' has void type" % name)
        if t.kind == "function":
            self.error(loc, "object '%s' has function type" % name)
        if is_global:
            if name in self.global_functions:
                self.error(loc, "'%s' redeclared as a different kind of symbol" % name)
            existing = self.current.ordinary.get(name)
            if existing is not None:
                if existing.kind != "object" or not type_eq(existing.type, t):
                    self.error(loc, "incompatible redeclaration of '%s'" % name)
                return
            symbol = Symbol(name, t, "object", loc, True, is_tentative=True)
            self.current.ordinary[name] = symbol
            self.tentative.append((symbol, loc))
            return
        if name in self.current.ordinary:
            self.error(loc, "redeclaration of '%s' with no linkage" % name)
        if not is_complete(t):
            self.error(loc, "object '%s' has incomplete type" % name)
        self.current.ordinary[name] = Symbol(name, t, "object", loc, False)

    # -- function definitions ------------------------------------------
    def analyze_function_definition(self, decl: Decl):
        base = self.resolve_type_spec(decl.spec)
        declarator = decl.declarator
        assert declarator is not None
        name = declarator.name
        if name is None:
            self.error(decl.loc, "function definition has no name")
        t = self.resolve_declarator(declarator, base)
        if t.kind != "function":
            self.error(decl.loc, "function definition does not declare a function")
        if t.result.kind == "struct":
            self.exempt(decl.loc, "function returning struct is exempted by Task E")
        for p in t.params:
            if p.kind == "struct":
                self.exempt(decl.loc, "function with struct parameter is exempted by Task E")
        if t.result.kind not in ("void",) and not is_complete(t.result):
            self.error(decl.loc, "function return type is incomplete")
        self.declare_function(name, t, declarator.loc, is_global=True,
                              is_definition=True)

        # The function name is visible in the body.
        self.function = FunctionContext(t.result)
        self.push_scope()
        self.bind_parameters(declarator, t)
        assert decl.body is not None
        self.analyze_compound(decl.body, push=False)
        for target, loc in self.function.pending_gotos:
            if target not in self.function.labels:
                self.error(loc, "use of undeclared label '%s'" % target)
        self.pop_scope()
        self.function = None

    def bind_parameters(self, declarator: Declarator, t: Type):
        # Find the outermost function constructor; its parameter Decls carry
        # the names.
        fn_ctor = None
        for ctor in declarator.ctors:
            if ctor.kind == "function":
                fn_ctor = ctor
                break
        if fn_ctor is None:
            return
        params = fn_ctor.params
        if len(params) == 1 and params[0].declarator is None:
            ptype = self.resolve_type_spec(params[0].spec)
            if ptype.kind == "void":
                return
        if len(params) != len(t.params):
            self.error(declarator.loc, "parameter list mismatch")
        for p, ptype in zip(params, t.params):
            if p.declarator is None or p.declarator.name is None:
                self.error(p.loc, "parameter in a function definition needs a name")
            name = p.declarator.name
            if name in self.current.ordinary:
                self.error(p.loc, "duplicate parameter '%s'" % name)
            if not is_complete(ptype):
                self.error(p.loc, "parameter '%s' has incomplete type" % name)
            self.current.ordinary[name] = Symbol(name, ptype, "object", p.loc, False)

    # -- statements ----------------------------------------------------
    def analyze_compound(self, stmt: Stmt, push: bool = True):
        if push:
            self.push_scope()
        for item in stmt.items:
            if item.kind == "decl":
                assert item.decl is not None
                self.analyze_declaration(item.decl, is_global=False)
            else:
                self.analyze_stmt(item)
        if push:
            self.pop_scope()

    def analyze_stmt(self, stmt: Stmt):
        if stmt.kind == "compound":
            self.analyze_compound(stmt, push=True)
        elif stmt.kind == "decl":
            assert stmt.decl is not None
            self.analyze_declaration(stmt.decl, is_global=False)
        elif stmt.kind == "expr":
            assert stmt.expr is not None
            self.check_expr(stmt.expr)
        elif stmt.kind == "empty":
            pass
        elif stmt.kind == "if":
            info = self.check_expr(stmt.cond)
            if not is_scalar(self.value_type(info)):
                self.error(stmt.loc, "if condition must have scalar type")
            self.analyze_stmt(stmt.then)
            if stmt.else_ is not None:
                self.analyze_stmt(stmt.else_)
        elif stmt.kind == "while":
            info = self.check_expr(stmt.cond)
            if not is_scalar(self.value_type(info)):
                self.error(stmt.loc, "while condition must have scalar type")
            assert self.function is not None
            self.function.loop_depth += 1
            self.analyze_stmt(stmt.body)
            self.function.loop_depth -= 1
        elif stmt.kind == "return":
            assert self.function is not None
            rt = self.function.return_type
            if stmt.expr is None:
                if rt.kind != "void":
                    self.error(stmt.loc, "return without a value in a non-void function")
            else:
                if rt.kind == "void":
                    self.error(stmt.loc, "return with a value in a void function")
                info = self.check_expr(stmt.expr)
                if self.value_type(info).kind == "struct" or rt.kind == "struct":
                    self.exempt(stmt.loc, "struct return is exempted by Task E")
                if not self.assignable(rt, info):
                    self.error(stmt.loc, "return value is not assignable to the return type")
        elif stmt.kind in ("break", "continue"):
            assert self.function is not None
            if self.function.loop_depth == 0:
                self.error(stmt.loc, "'%s' outside a loop" % stmt.kind)
        elif stmt.kind == "goto":
            assert self.function is not None
            target = stmt.target or ""
            if target in self.function.labels:
                pass
            else:
                self.function.pending_gotos.append((target, stmt.loc))
        elif stmt.kind == "label":
            assert self.function is not None
            label = stmt.label or ""
            if label in self.function.labels:
                self.error(stmt.loc, "duplicate label '%s'" % label)
            self.function.labels[label] = stmt.loc
            self.analyze_stmt(stmt.body)
        else:
            self.error(stmt.loc, "unknown statement")

    # -- expressions ---------------------------------------------------
    def value_type(self, info: ExprInfo) -> Type:
        if info.type.kind == "array":
            return ptr(info.type.elem)
        if info.category == "function":
            return ptr(info.type)
        return info.type

    def check_expr(self, expr: Expr) -> ExprInfo:
        kind = expr.kind
        if kind == "identifier":
            return self.check_identifier(expr)
        if kind == "constant":
            return self.check_constant(expr)
        if kind == "string":
            return self.check_string(expr)
        if kind == "paren":
            inner = self.check_expr(expr.operand)
            return ExprInfo(inner.type, inner.category, False, inner.is_const,
                            inner.value, inner.symbol, inner.field, inner.label)
        if kind == "unary":
            return self.check_unary(expr)
        if kind == "binary":
            return self.check_binary(expr)
        if kind == "assign":
            return self.check_assign(expr)
        if kind == "conditional":
            return self.check_conditional(expr)
        if kind == "call":
            return self.check_call(expr)
        if kind == "subscript":
            return self.check_subscript(expr)
        if kind == "member":
            return self.check_member(expr)
        if kind == "sizeof_expr":
            return self.check_sizeof_expr(expr)
        if kind == "sizeof_type":
            return self.check_sizeof_type(expr)
        self.error(expr.loc, "unknown expression")

    def check_identifier(self, expr: Expr) -> ExprInfo:
        name = expr.name or ""
        if name == "__func__":
            self.exempt(expr.loc, "__func__ is exempted by Task E")
        symbol = self.current.lookup_ordinary(name)
        if symbol is None:
            self.error(expr.loc, "use of undeclared identifier '%s'" % name)
        if symbol.kind == "function":
            return ExprInfo(symbol.type, "function", symbol=symbol)
        return ExprInfo(symbol.type, "lvalue", symbol=symbol)

    def check_constant(self, expr: Expr) -> ExprInfo:
        text = expr.text or ""
        if text.startswith("'"):
            value = decode_char_constant(text)
            return ExprInfo(T_INT, "rvalue", is_const=True, value=value)
        value = int(text, 10)
        return ExprInfo(T_INT, "rvalue", is_null=(text == "0"),
                        is_const=True, value=value)

    def check_string(self, expr: Expr) -> ExprInfo:
        text = expr.text or '""'
        body = text[1:-1]
        count = 0
        i = 0
        while i < len(body):
            if body[i] == "\\":
                i += 2
            else:
                i += 1
            count += 1
        return ExprInfo(array(T_CHAR, count + 1), "lvalue", is_const=True)

    def check_unary(self, expr: Expr) -> ExprInfo:
        op = expr.op or ""
        info = self.check_expr(expr.operand)
        if op == "&":
            if info.category == "function":
                return ExprInfo(ptr(info.type), "rvalue")
            if info.category == "lvalue" and is_object(info.type):
                return ExprInfo(ptr(info.type), "rvalue")
            self.error(expr.loc, "operand of '&' is not an lvalue or function designator")
        if op == "*":
            t = self.value_type(info)
            if t.kind != "pointer":
                self.error(expr.loc, "operand of unary '*' is not a pointer")
            if t.elem.kind == "function":
                return ExprInfo(t.elem, "function")
            if t.elem.kind == "void":
                return ExprInfo(T_VOID, "void")
            return ExprInfo(t.elem, "lvalue")
        if op == "-":
            t = self.value_type(info)
            if not is_arithmetic(t):
                self.error(expr.loc, "operand of unary '-' is not arithmetic")
            return ExprInfo(T_INT, "rvalue")
        if op == "!":
            t = self.value_type(info)
            if not is_scalar(t):
                self.error(expr.loc, "operand of '!' is not scalar")
            return ExprInfo(T_INT, "rvalue")
        self.error(expr.loc, "unknown unary operator")

    def check_binary(self, expr: Expr) -> ExprInfo:
        op = expr.op or ""
        li = self.check_expr(expr.lhs)
        ri = self.check_expr(expr.rhs)
        lt = self.value_type(li)
        rt = self.value_type(ri)
        loc = expr.op_loc or expr.loc

        if op == "*":
            if not (is_arithmetic(lt) and is_arithmetic(rt)):
                self.error(loc, "operands of '*' must be arithmetic")
            return ExprInfo(T_INT, "rvalue")

        if op == "+":
            if is_arithmetic(lt) and is_arithmetic(rt):
                return ExprInfo(T_INT, "rvalue")
            if lt.kind == "pointer" and is_arithmetic(rt):
                self.require_complete_object(lt.elem, loc, "pointer arithmetic")
                return ExprInfo(lt, "rvalue")
            if rt.kind == "pointer" and is_arithmetic(lt):
                self.require_complete_object(rt.elem, loc, "pointer arithmetic")
                return ExprInfo(rt, "rvalue")
            self.error(loc, "invalid operands to '+'")

        if op == "-":
            if is_arithmetic(lt) and is_arithmetic(rt):
                return ExprInfo(T_INT, "rvalue")
            if lt.kind == "pointer" and is_arithmetic(rt):
                self.require_complete_object(lt.elem, loc, "pointer arithmetic")
                return ExprInfo(lt, "rvalue")
            if lt.kind == "pointer" and rt.kind == "pointer":
                if not (type_eq(lt.elem, rt.elem) and is_object(lt.elem)):
                    self.error(loc, "pointer subtraction requires compatible complete object types")
                self.require_complete_object(lt.elem, loc, "pointer subtraction")
                return ExprInfo(T_INT, "rvalue")
            self.error(loc, "invalid operands to '-'")

        if op == "<":
            if is_arithmetic(lt) and is_arithmetic(rt):
                return ExprInfo(T_INT, "rvalue")
            if lt.kind == "pointer" and rt.kind == "pointer":
                if type_eq(lt.elem, rt.elem) and is_object(lt.elem):
                    return ExprInfo(T_INT, "rvalue")
            self.error(loc, "invalid operands to '<'")

        if op in ("==", "!="):
            if is_arithmetic(lt) and is_arithmetic(rt):
                return ExprInfo(T_INT, "rvalue")
            if lt.kind == "pointer" and ri.is_null:
                return ExprInfo(T_INT, "rvalue")
            if rt.kind == "pointer" and li.is_null:
                return ExprInfo(T_INT, "rvalue")
            if lt.kind == "pointer" and rt.kind == "pointer":
                if type_eq(lt.elem, rt.elem):
                    return ExprInfo(T_INT, "rvalue")
                if (lt.elem.kind == "void" and is_object(rt.elem)) or \
                   (rt.elem.kind == "void" and is_object(lt.elem)):
                    return ExprInfo(T_INT, "rvalue")
            self.error(loc, "invalid operands to '%s'" % op)

        if op in ("&&", "||"):
            if is_scalar(lt) and is_scalar(rt):
                return ExprInfo(T_INT, "rvalue")
            self.error(loc, "operands of '%s' must be scalar" % op)

        self.error(loc, "unknown binary operator")

    def check_conditional(self, expr: Expr) -> ExprInfo:
        ci = self.check_expr(expr.cond)
        if not is_scalar(self.value_type(ci)):
            self.error(expr.loc, "conditional condition must have scalar type")
        ti = self.check_expr(expr.then)
        ei = self.check_expr(expr.else_)
        tt = self.value_type(ti)
        et = self.value_type(ei)
        loc = expr.op_loc or expr.loc
        if is_arithmetic(tt) and is_arithmetic(et):
            return ExprInfo(T_INT, "rvalue")
        if tt.kind == "struct" and et.kind == "struct" and type_eq(tt, et):
            return ExprInfo(tt, "rvalue")
        if tt.kind == "void" and et.kind == "void":
            return ExprInfo(T_VOID, "void")
        if tt.kind == "pointer" and ei.is_null:
            return ExprInfo(tt, "rvalue")
        if et.kind == "pointer" and ti.is_null:
            return ExprInfo(et, "rvalue")
        if tt.kind == "pointer" and et.kind == "pointer":
            if type_eq(tt.elem, et.elem):
                return ExprInfo(tt, "rvalue")
            if (tt.elem.kind == "void" and is_object(et.elem)) or \
               (et.elem.kind == "void" and is_object(tt.elem)):
                return ExprInfo(ptr(T_VOID), "rvalue")
        self.error(loc, "incompatible operands to conditional expression")

    def check_assign(self, expr: Expr) -> ExprInfo:
        li = self.check_expr(expr.lhs)
        if li.category != "lvalue":
            self.error(expr.op_loc or expr.loc, "left operand of '=' is not an lvalue")
        lt = li.type
        if not is_object(lt) or not is_complete(lt):
            self.error(expr.op_loc or expr.loc, "left operand of '=' is not a complete object")
        if lt.kind == "array" or lt.kind == "function":
            self.error(expr.op_loc or expr.loc, "left operand of '=' is not modifiable")
        ri = self.check_expr(expr.rhs)
        if lt.kind == "struct" or self.value_type(ri).kind == "struct":
            self.exempt(expr.op_loc or expr.loc, "struct assignment is exempted by Task E")
        if not self.assignable(lt, ri):
            self.error(expr.op_loc or expr.loc,
                       "right operand is not assignable to '%s'" % type_str(lt))
        return ExprInfo(lt, "rvalue")

    def check_call(self, expr: Expr) -> ExprInfo:
        callee = self.check_expr(expr.callee)
        ct = self.value_type(callee)
        if ct.kind == "function":
            ft = ct
        elif ct.kind == "pointer" and ct.elem.kind == "function":
            ft = ct.elem
        else:
            self.error(expr.loc, "called object is not a function")
        if ft.result.kind not in ("void",) and not is_complete(ft.result):
            self.error(expr.loc, "called function returns an incomplete type")
        if ft.result.kind in ("function", "array"):
            self.error(expr.loc, "called function has invalid return type")
        if len(expr.args) != len(ft.params):
            self.error(expr.op_loc or expr.loc,
                       "call has %d arguments but function expects %d"
                       % (len(expr.args), len(ft.params)))
        for arg, param in zip(expr.args, ft.params):
            ai = self.check_expr(arg)
            if param.kind == "struct" or self.value_type(ai).kind == "struct":
                self.exempt(arg.loc, "struct argument is exempted by Task E")
            if not self.assignable(param, ai):
                self.error(arg.loc, "argument is not assignable to parameter type '%s'"
                           % type_str(param))
        if ft.result.kind == "void":
            return ExprInfo(T_VOID, "void")
        return ExprInfo(ft.result, "rvalue")

    def check_subscript(self, expr: Expr) -> ExprInfo:
        bi = self.check_expr(expr.base)
        ii = self.check_expr(expr.index)
        bt = self.value_type(bi)
        it = self.value_type(ii)
        if bt.kind == "pointer" and is_arithmetic(it):
            elem = bt.elem
        elif it.kind == "pointer" and is_arithmetic(bt):
            elem = it.elem
        else:
            self.error(expr.loc, "subscript requires a pointer and an integer")
        self.require_complete_object(elem, expr.loc, "subscript")
        return ExprInfo(elem, "lvalue")

    def check_member(self, expr: Expr) -> ExprInfo:
        base = self.check_expr(expr.base)
        bt = self.value_type(base)
        if expr.arrow:
            if bt.kind != "pointer" or bt.elem.kind != "struct":
                self.error(expr.loc, "'->' requires a pointer to struct")
            info = bt.elem.struct
            category = "lvalue"
        else:
            if bt.kind != "struct":
                self.error(expr.loc, "'.' requires a struct operand")
            info = bt.struct
            category = "lvalue" if base.category == "lvalue" else "rvalue"
        if not info.complete:
            self.error(expr.loc, "member access on incomplete struct")
        for f in info.fields:
            if f.name == expr.member:
                return ExprInfo(f.type, category, field=f)
        self.error(expr.loc, "struct has no member '%s'" % (expr.member or ""))

    def check_sizeof_expr(self, expr: Expr) -> ExprInfo:
        info = self.check_expr(expr.operand)
        t = info.type
        if t.kind in ("function", "void") or not is_complete(t):
            self.error(expr.loc, "sizeof operand has incomplete or function type")
        return ExprInfo(T_INT, "rvalue", is_const=True, value=self.size_of(t))

    def check_sizeof_type(self, expr: Expr) -> ExprInfo:
        base = self.resolve_type_spec(expr.type_spec)
        if expr.type_decl is not None:
            base = self.resolve_declarator(expr.type_decl, base)
        if base.kind in ("function", "void") or not is_complete(base):
            self.error(expr.loc, "sizeof type is incomplete or a function type")
        return ExprInfo(T_INT, "rvalue", is_const=True, value=self.size_of(base))

    # -- helpers -------------------------------------------------------
    def require_complete_object(self, t: Type, loc: Loc, what: str):
        if not is_object(t) or not is_complete(t):
            self.error(loc, "%s requires a pointer to a complete object type" % what)

    def assignable(self, to: Type, from_info: ExprInfo) -> bool:
        frm = self.value_type(from_info)
        if is_arithmetic(to) and is_arithmetic(frm):
            return True
        if to.kind == "pointer":
            if from_info.is_null:
                return True
            if frm.kind == "pointer":
                if type_eq(to.elem, frm.elem):
                    return True
                if (to.elem.kind == "void" and is_object(frm.elem)) or \
                   (frm.elem.kind == "void" and is_object(to.elem)):
                    return True
        if to.kind == "struct" and frm.kind == "struct" and type_eq(to, frm):
            return True
        return False

    def size_of(self, t: Type) -> int:
        if t.kind == "char":
            return 1
        if t.kind == "int":
            return 4
        if t.kind == "pointer":
            return 8
        if t.kind == "array":
            return t.bound * self.size_of(t.elem)
        if t.kind == "struct":
            if not t.struct.complete:
                self.error(t.struct.loc, "sizeof incomplete struct")
            offset = 0
            align = 1
            for f in t.struct.fields:
                fa = self.align_of(f.type)
                align = max(align, fa)
                offset = (offset + fa - 1) // fa * fa
                offset += self.size_of(f.type)
            return (offset + align - 1) // align * align
        self.error(t.struct.loc if t.struct else Loc(1, 1), "cannot compute size")

    def align_of(self, t: Type) -> int:
        if t.kind == "char":
            return 1
        if t.kind == "int":
            return 4
        if t.kind == "pointer":
            return 8
        if t.kind == "array":
            return self.align_of(t.elem)
        if t.kind == "struct":
            align = 1
            for f in t.struct.fields:
                align = max(align, self.align_of(f.type))
            return align
        return 1


def decode_char_constant(text: str) -> int:
    body = text[1:-1]
    if body.startswith("\\"):
        esc = body[1]
        return {
            "'": ord("'"), '"': ord('"'), "?": ord("?"), "\\": ord("\\"),
            "a": 7, "b": 8, "f": 12, "n": 10, "r": 13, "t": 9, "v": 11,
        }[esc]
    return ord(body)


def check(program: Program) -> SemaResult:
    return Sema(program).analyze()

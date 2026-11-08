"""AST for the C4 subset, produced by the independent reference parser."""

from dataclasses import dataclass, field
from typing import List, Optional


@dataclass
class Loc:
    line: int
    column: int


@dataclass
class Ctor:
    kind: str                       # "pointer" | "function"
    loc: Loc
    params: List["Decl"] = field(default_factory=list)


@dataclass
class Declarator:
    name: Optional[str]
    ctors: List[Ctor]
    loc: Loc


@dataclass
class TypeSpec:
    kind: str                       # "void" | "char" | "int" | "struct"
    loc: Loc
    tag: Optional[str] = None
    fields: List["Decl"] = field(default_factory=list)
    has_braces: bool = False


@dataclass
class Decl:
    spec: TypeSpec
    declarator: Optional[Declarator]
    loc: Loc
    body: Optional["Stmt"] = None   # function definition body


@dataclass
class Expr:
    kind: str
    loc: Loc
    name: Optional[str] = None
    text: Optional[str] = None
    op: Optional[str] = None
    op_loc: Optional[Loc] = None
    operand: Optional["Expr"] = None
    lhs: Optional["Expr"] = None
    rhs: Optional["Expr"] = None
    cond: Optional["Expr"] = None
    then: Optional["Expr"] = None
    else_: Optional["Expr"] = None
    callee: Optional["Expr"] = None
    args: List["Expr"] = field(default_factory=list)
    base: Optional["Expr"] = None
    index: Optional["Expr"] = None
    member: Optional[str] = None
    arrow: bool = False
    type_spec: Optional[TypeSpec] = None
    type_decl: Optional[Declarator] = None


@dataclass
class Stmt:
    kind: str
    loc: Loc
    items: List["Stmt"] = field(default_factory=list)
    decl: Optional[Decl] = None
    expr: Optional[Expr] = None
    cond: Optional[Expr] = None
    then: Optional["Stmt"] = None
    else_: Optional["Stmt"] = None
    body: Optional["Stmt"] = None
    label: Optional[str] = None
    target: Optional[str] = None


@dataclass
class Program:
    decls: List[Decl]

"""Reference recursive-descent parser for the C4 subset.

Derived from verification/spec/grammar.ebnf (Assignment 2, Task C).
The parser is independent of the C++ implementation.  It builds the AST in
c4ref/ast.py; semantic analysis is in c4ref/sema.py.
"""

from typing import List, Optional

from .ast import Ctor, Decl, Declarator, Expr, Loc, Program, Stmt, TypeSpec
from .lexer import Token


DIGRAPHS = {
    "<:": "[", ":>": "]", "<%": "{", "%>": "}", "%:": "#", "%:%:": "##",
}
TYPE_SPECIFIERS = {"void", "char", "int", "struct"}


class ParseError(Exception):
    def __init__(self, loc: Loc, message: str):
        super().__init__(message)
        self.loc = loc
        self.message = message


class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0

    # ------------------------------------------------------------------
    # Token helpers
    # ------------------------------------------------------------------
    def peek(self, k: int = 0) -> Token:
        j = self.pos + k
        if j >= len(self.tokens):
            return self.tokens[-1]
        return self.tokens[j]

    def canon(self, tok: Optional[Token] = None) -> str:
        tok = tok or self.peek()
        if tok.kind == "punctuator":
            return DIGRAPHS.get(tok.text, tok.text)
        return tok.text

    def at(self, text: str) -> bool:
        return self.canon() == text

    def at_kind(self, kind: str) -> bool:
        return self.peek().kind == kind

    def starts_type_specifier(self, tok: Optional[Token] = None) -> bool:
        return self.canon(tok) in TYPE_SPECIFIERS

    def advance(self) -> Token:
        tok = self.peek()
        if tok.kind != "eof":
            self.pos += 1
        return tok

    def expect(self, text: str) -> Token:
        tok = self.peek()
        if self.canon(tok) != text:
            raise ParseError(tok_loc(tok), "expected %r but got %r" % (text, self.describe(tok)))
        return self.advance()

    def expect_identifier(self) -> Token:
        tok = self.peek()
        if tok.kind != "identifier":
            raise ParseError(tok_loc(tok), "expected identifier but got %r" % self.describe(tok))
        return self.advance()

    @staticmethod
    def describe(tok: Token) -> str:
        if tok.kind == "eof":
            return "end of file"
        return tok.text

    # ------------------------------------------------------------------
    # Translation unit and declarations
    # ------------------------------------------------------------------
    def parse_translation_unit(self) -> Program:
        decls: List[Decl] = []
        while not self.at_kind("eof"):
            decls.append(self.parse_external_declaration())
        if not decls:
            raise ParseError(tok_loc(self.peek()), "expected external declaration")
        return Program(decls)

    def parse_external_declaration(self) -> Decl:
        spec = self.parse_type_specifier()
        loc = spec.loc
        if self.at(";"):
            self.advance()
            return Decl(spec, None, loc)
        declarator = self.parse_declarator(allow_abstract=False)
        if self.at("{"):
            body = self.parse_compound_statement()
            return Decl(spec, declarator, loc, body)
        self.expect(";")
        return Decl(spec, declarator, loc)

    def parse_declaration(self) -> Decl:
        spec = self.parse_type_specifier()
        loc = spec.loc
        if self.at(";"):
            self.advance()
            return Decl(spec, None, loc)
        declarator = self.parse_declarator(allow_abstract=False)
        self.expect(";")
        return Decl(spec, declarator, loc)

    def parse_type_specifier(self) -> TypeSpec:
        tok = self.peek()
        text = self.canon(tok)
        if text in ("void", "char", "int"):
            self.advance()
            return TypeSpec(text, tok_loc(tok))
        if text == "struct":
            loc = tok_loc(tok)
            self.advance()
            tag: Optional[str] = None
            if self.at_kind("identifier"):
                tag = self.advance().text
            if self.at("{"):
                self.advance()  # "{"
                fields = self.parse_struct_declaration_list()
                self.expect("}")
                return TypeSpec("struct", loc, tag=tag, fields=fields, has_braces=True)
            if tag is None:
                raise ParseError(tok_loc(self.peek()), "expected struct tag or body")
            return TypeSpec("struct", loc, tag=tag, fields=[], has_braces=False)
        raise ParseError(tok_loc(tok), "expected type specifier but got %r" % self.describe(tok))

    def parse_struct_declaration_list(self) -> List[Decl]:
        fields: List[Decl] = []
        if self.at("}"):
            raise ParseError(tok_loc(self.peek()), "empty struct declaration list")
        while not self.at("}"):
            if self.at_kind("eof"):
                raise ParseError(tok_loc(self.peek()), "expected '}'")
            fields.extend(self.parse_struct_declaration())
        return fields

    def parse_struct_declaration(self) -> List[Decl]:
        spec = self.parse_type_specifier()
        loc = spec.loc
        decls: List[Decl] = []
        if self.at(";"):
            self.advance()
            return [Decl(spec, None, loc)]
        while True:
            declarator = self.parse_declarator(allow_abstract=False)
            decls.append(Decl(spec, declarator, declarator.loc))
            if self.at(","):
                self.advance()
                continue
            break
        self.expect(";")
        return decls

    # ------------------------------------------------------------------
    # Declarators
    # ------------------------------------------------------------------
    def parse_declarator(self, allow_abstract: bool) -> Declarator:
        start = tok_loc(self.peek())
        pointers: List[Loc] = []
        while self.at("*"):
            pointers.append(tok_loc(self.advance()))

        name: Optional[str] = None
        ctors: List[Ctor] = []

        if self.at_kind("identifier"):
            name = self.advance().text
        elif self.at("("):
            # In an abstract context, "( type-specifier ..." begins a
            # parameter-type-list.  Otherwise it is a parenthesized declarator.
            if allow_abstract and self.starts_type_specifier(self.peek(1)):
                lparen = tok_loc(self.advance())
                params = self.parse_parameter_type_list()
                self.expect(")")
                ctors = [Ctor("function", lparen, params)]
            else:
                self.advance()  # "("
                inner = self.parse_declarator(allow_abstract)
                self.expect(")")
                name = inner.name
                ctors = list(inner.ctors)
        elif allow_abstract:
            # Empty abstract declarator.
            pass
        else:
            raise ParseError(tok_loc(self.peek()), "expected declarator but got %r" % self.describe(self.peek()))

        # Postfix function declarators bind more tightly than the pointer
        # prefix and are recorded outward from the identifier.
        while self.at("("):
            lparen = tok_loc(self.advance())
            params = self.parse_parameter_type_list()
            self.expect(")")
            ctors.append(Ctor("function", lparen, params))

        # The pointer prefix is outside the direct declarator, hence later in
        # the outward list (e.g. *f(int) is function returning pointer).
        for p in pointers:
            ctors.append(Ctor("pointer", p))

        return Declarator(name, ctors, start)

    def parse_parameter_type_list(self) -> List[Decl]:
        if self.at(")"):
            raise ParseError(tok_loc(self.peek()), "parameter-type-list cannot be empty")
        params: List[Decl] = []
        while True:
            params.append(self.parse_parameter_declaration())
            if self.at(","):
                self.advance()
                continue
            break
        return params

    def parse_parameter_declaration(self) -> Decl:
        spec = self.parse_type_specifier()
        loc = spec.loc
        if self.at(",") or self.at(")"):
            return Decl(spec, None, loc)
        declarator = self.parse_declarator(allow_abstract=True)
        return Decl(spec, declarator, loc)

    def parse_type_name(self):
        spec = self.parse_type_specifier()
        if self.at(")"):
            return spec, None
        declarator = self.parse_declarator(allow_abstract=True)
        return spec, declarator

    # ------------------------------------------------------------------
    # Expressions
    # ------------------------------------------------------------------
    def parse_expression(self) -> Expr:
        return self.parse_assignment_expression()

    def parse_assignment_expression(self) -> Expr:
        lhs = self.parse_conditional_expression()
        if self.at("="):
            op = self.advance()
            rhs = self.parse_assignment_expression()
            return Expr("assign", lhs.loc, lhs=lhs, rhs=rhs, op="=", op_loc=tok_loc(op))
        return lhs

    def parse_conditional_expression(self) -> Expr:
        cond = self.parse_logical_or_expression()
        if self.at("?"):
            op = self.advance()
            then_expr = self.parse_expression()
            self.expect(":")
            else_expr = self.parse_conditional_expression()
            return Expr("conditional", cond.loc, cond=cond, then=then_expr,
                        else_=else_expr, op="?:", op_loc=tok_loc(op))
        return cond

    def parse_logical_or_expression(self) -> Expr:
        lhs = self.parse_logical_and_expression()
        while self.at("||"):
            op = self.advance()
            rhs = self.parse_logical_and_expression()
            lhs = Expr("binary", lhs.loc, op="||", op_loc=tok_loc(op), lhs=lhs, rhs=rhs)
        return lhs

    def parse_logical_and_expression(self) -> Expr:
        lhs = self.parse_equality_expression()
        while self.at("&&"):
            op = self.advance()
            rhs = self.parse_equality_expression()
            lhs = Expr("binary", lhs.loc, op="&&", op_loc=tok_loc(op), lhs=lhs, rhs=rhs)
        return lhs

    def parse_equality_expression(self) -> Expr:
        lhs = self.parse_relational_expression()
        while self.at("==") or self.at("!="):
            op = self.advance()
            rhs = self.parse_relational_expression()
            lhs = Expr("binary", lhs.loc, op=self.canon(op), op_loc=tok_loc(op), lhs=lhs, rhs=rhs)
        return lhs

    def parse_relational_expression(self) -> Expr:
        lhs = self.parse_additive_expression()
        while self.at("<"):
            op = self.advance()
            rhs = self.parse_additive_expression()
            lhs = Expr("binary", lhs.loc, op="<", op_loc=tok_loc(op), lhs=lhs, rhs=rhs)
        return lhs

    def parse_additive_expression(self) -> Expr:
        lhs = self.parse_multiplicative_expression()
        while self.at("+") or self.at("-"):
            op = self.advance()
            rhs = self.parse_multiplicative_expression()
            lhs = Expr("binary", lhs.loc, op=self.canon(op), op_loc=tok_loc(op), lhs=lhs, rhs=rhs)
        return lhs

    def parse_multiplicative_expression(self) -> Expr:
        lhs = self.parse_unary_expression()
        while self.at("*"):
            op = self.advance()
            rhs = self.parse_unary_expression()
            lhs = Expr("binary", lhs.loc, op="*", op_loc=tok_loc(op), lhs=lhs, rhs=rhs)
        return lhs

    def parse_unary_expression(self) -> Expr:
        if self.at("&") or self.at("*") or self.at("-") or self.at("!"):
            op = self.advance()
            operand = self.parse_unary_expression()
            return Expr("unary", tok_loc(op), op=self.canon(op),
                        operand=operand)
        if self.at("sizeof"):
            tok = self.advance()
            if self.at("(") and self.starts_type_specifier(self.peek(1)):
                self.advance()  # "("
                spec, declarator = self.parse_type_name()
                self.expect(")")
                return Expr("sizeof_type", tok_loc(tok), type_spec=spec,
                            type_decl=declarator)
            operand = self.parse_unary_expression()
            return Expr("sizeof_expr", tok_loc(tok), operand=operand)
        return self.parse_postfix_expression()

    def parse_postfix_expression(self) -> Expr:
        expr = self.parse_primary_expression()
        while True:
            if self.at("["):
                self.advance()
                index = self.parse_expression()
                self.expect("]")
                expr = Expr("subscript", expr.loc, base=expr, index=index)
            elif self.at("("):
                lparen = self.advance()
                args: List[Expr] = []
                if not self.at(")"):
                    while True:
                        args.append(self.parse_assignment_expression())
                        if self.at(","):
                            self.advance()
                            continue
                        break
                self.expect(")")
                expr = Expr("call", expr.loc, callee=expr, args=args,
                            op_loc=tok_loc(lparen))
            elif self.at("."):
                self.advance()
                member = self.expect_identifier().text
                expr = Expr("member", expr.loc, base=expr, member=member,
                            arrow=False)
            elif self.at("->"):
                self.advance()
                member = self.expect_identifier().text
                expr = Expr("member", expr.loc, base=expr, member=member,
                            arrow=True)
            else:
                break
        return expr

    def parse_primary_expression(self) -> Expr:
        tok = self.peek()
        if tok.kind == "identifier":
            self.advance()
            return Expr("identifier", tok_loc(tok), name=tok.text)
        if tok.kind == "constant":
            self.advance()
            return Expr("constant", tok_loc(tok), text=tok.text)
        if tok.kind == "string-literal":
            self.advance()
            return Expr("string", tok_loc(tok), text=tok.text)
        if self.at("("):
            self.advance()
            expr = self.parse_expression()
            self.expect(")")
            return Expr("paren", tok_loc(tok), operand=expr)
        raise ParseError(tok_loc(tok), "expected expression but got %r" % self.describe(tok))

    # ------------------------------------------------------------------
    # Statements
    # ------------------------------------------------------------------
    def parse_compound_statement(self) -> Stmt:
        loc = tok_loc(self.expect("{"))
        items: List[Stmt] = []
        while not self.at("}"):
            if self.at_kind("eof"):
                raise ParseError(tok_loc(self.peek()), "expected '}'")
            if self.starts_type_specifier():
                decl = self.parse_declaration()
                items.append(Stmt("decl", decl.loc, decl=decl))
            else:
                items.append(self.parse_statement())
        self.expect("}")
        return Stmt("compound", loc, items=items)

    def parse_statement(self) -> Stmt:
        tok = self.peek()
        if self.at("{"):
            return self.parse_compound_statement()
        if self.at("if"):
            loc = tok_loc(self.advance())
            self.expect("(")
            cond = self.parse_expression()
            self.expect(")")
            then_stmt = self.parse_statement()
            else_stmt = None
            if self.at("else"):
                self.advance()
                else_stmt = self.parse_statement()
            return Stmt("if", loc, cond=cond, then=then_stmt, else_=else_stmt)
        if self.at("while"):
            loc = tok_loc(self.advance())
            self.expect("(")
            cond = self.parse_expression()
            self.expect(")")
            body = self.parse_statement()
            return Stmt("while", loc, cond=cond, body=body)
        if self.at("return"):
            loc = tok_loc(self.advance())
            expr = None
            if not self.at(";"):
                expr = self.parse_expression()
            self.expect(";")
            return Stmt("return", loc, expr=expr)
        if self.at("break") or self.at("continue"):
            loc = tok_loc(self.advance())
            self.expect(";")
            return Stmt("break" if tok.text == "break" else "continue", loc)
        if self.at("goto"):
            loc = tok_loc(self.advance())
            target = self.expect_identifier().text
            self.expect(";")
            return Stmt("goto", loc, target=target)
        if self.at(";"):
            self.advance()
            return Stmt("empty", tok_loc(tok))
        if tok.kind == "identifier" and self.canon(self.peek(1)) == ":":
            self.advance()
            self.expect(":")
            stmt = self.parse_statement()
            return Stmt("label", tok_loc(tok), label=tok.text, body=stmt)
        expr = self.parse_expression()
        self.expect(";")
        return Stmt("expr", expr.loc, expr=expr)


def tok_loc(tok: Token) -> Loc:
    return Loc(tok.line, tok.column)


def parse(tokens: List[Token]) -> Program:
    return Parser(tokens).parse_translation_unit()

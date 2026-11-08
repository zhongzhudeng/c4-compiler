"""Parser conformance: curated grammar cases plus token-deletion mutants.

Valid cases are parsed and semantically checked by the reference frontend and
must be accepted by c4 --parse.  Invalid cases must be rejected by both.
Valid C outside the C4 grammar is classified may-reject: rejection is
conforming, and acceptance is recorded as an extension.
"""

from __future__ import annotations

from typing import Dict, List

from .runner import Checks, Context, Reporter, DIAGNOSTIC_RE, reference_tokens

VALID_CASES: List[Dict[str, object]] = [
    {
        "name": "declarations",
        "source": """int x;
char c;
void f(void);
struct S;
struct S { int a; char b; };
struct T { struct S s; struct T *next; } t;
struct { int x; } anon;
int *p;
int **pp;
int (*fp)(int);
int *g(int x);
int (h)(int y);
""",
        "requirements": ["C-001", "C-003", "C-004", "C-006", "C-009"],
    },
    {
        "name": "function-definitions",
        "source": """int add(int a, int b) { return a + b; }
void sink(int x) { }
int main(void) { return add(1, 2); }
""",
        "requirements": ["C-006", "C-009"],
    },
    {
        "name": "expressions",
        "source": """int main(void) {
  int a; int b; int *p; struct S { int x; } s;
  a = 1; b = 2;
  a = a * b + a - b;
  a = (a < b) == (b != a);
  a = a && b || !a;
  a = a ? b : 3;
  p = &a; *p = 5; a = *p; p = p + 1; p = 1 + p; a = p == p; a = p != p;
  s.x = a; a = s.x;
  a = sizeof a; a = sizeof(int); a = sizeof(struct S); a = sizeof(int *);
  return a;
}
""",
        "requirements": ["C-001", "C-009"],
    },
    {
        "name": "statements",
        "source": """int main(void) {
  int i; int x;
  i = 0; x = 0;
  while (i < 3) {
    if (i == 1) { x = x + 1; }
    else if (i == 2) { x = x + 2; }
    else x = x + 3;
    i = i + 1;
  }
  goto done;
  x = 1;
done:
  ;
  return x;
}
""",
        "requirements": ["C-005", "C-009"],
    },
    {
        "name": "break-and-continue",
        "source": """int main(void) {
  int i; int x;
  i = 0; x = 0;
  while (i < 3) {
    i = i + 1;
    if (i == 1) continue;
    if (i == 2) break;
    x = x + 1;
  }
  return x;
}
""",
        "requirements": ["C-005", "C-009"],
    },
    {
        "name": "dangling-else",
        "source": """int main(void) {
  int a;
  a = 1;
  if (a) if (a) a = 2; else a = 3;
  return a;
}
""",
        "requirements": ["C-005", "C-011"],
    },
    {
        "name": "nested-scopes",
        "source": """int main(void) {
  int x;
  x = 0;
  { int x; x = 1; }
  return x;
}
""",
        "requirements": ["C-009"],
    },
    {
        "name": "struct-member-chain",
        "source": """struct S { int x; };
int main(void) {
  struct S s; struct S *p;
  p = &s; s.x = 1; p->x = 2;
  return s.x;
}
""",
        "requirements": ["C-001", "C-009"],
    },
    {
        "name": "function-pointer-call",
        "source": """int f(int x) { return x; }
int main(void) {
  int (*p)(int);
  p = f;
  return p(3);
}
""",
        "requirements": ["C-001", "C-004", "C-009"],
    },
    {
        "name": "recursion",
        "source": """int f(int n) {
  if (n < 2) return n;
  return f(n - 1) + f(n - 2);
}
int main(void) { return f(5); }
""",
        "requirements": ["C-009"],
    },
    {
        "name": "void-call",
        "source": """void g(void) { }
int main(void) { g(); return 0; }
""",
        "requirements": ["C-009"],
    },
    {
        "name": "char-int-conversions",
        "source": """int main(void) {
  char c; int i;
  c = 65;
  i = c;
  return i;
}
""",
        "requirements": ["C-001", "C-009"],
    },
    {
        "name": "string-subscript",
        "source": """int main(void) {
  char *p;
  p = "abc";
  return p[0];
}
""",
        "requirements": ["C-001", "C-009"],
    },
    {
        "name": "conditional-void",
        "source": """void g(void) { }
int main(void) {
  int a;
  a = 1;
  a ? g() : g();
  return 0;
}
""",
        "requirements": ["C-001", "C-009"],
    },
    {
        "name": "forward-pointer",
        "source": """struct S;
struct S *p;
int main(void) { return 0; }
""",
        "requirements": ["C-003", "C-009"],
    },
    {
        "name": "tentative-definitions",
        "source": """int x;
int x;
int main(void) { return x; }
""",
        "requirements": ["C-003", "C-009"],
    },
    {
        "name": "block-function-declaration",
        "source": """int f(void) { return 3; }
int main(void) {
  int f(void);
  return f();
}
""",
        "requirements": ["C-003", "C-004", "C-009"],
    },
    {
        "name": "prototype-scope",
        "source": """int f(int x);
int main(void) { return f(1); }
int f(int x) { return x; }
""",
        "requirements": ["C-004", "C-009"],
    },
    {
        "name": "null-statements-and-labels",
        "source": """int main(void) {
  ;
  label: ;
  return 0;
}
""",
        "requirements": ["C-005", "C-009"],
    },
    {
        "name": "digraph-braces",
        "source": """int main(void) <% return 0; %>
""",
        "requirements": ["C-009", "L-009"],
    },
]


def _generated_valid_cases():
    """Systematic valid programs assembled from grammar productions."""
    cases = []
    expressions = [
        "a", "b", "(a)", "a * b", "a + b", "a - b", "a < b", "a == b",
        "a != b", "a && b", "a || b", "!a", "-a", "a ? b : a",
        "sizeof a", "sizeof(int)", "sizeof(int *)", "(a = b)",
    ]
    for index, expression in enumerate(expressions):
        cases.append({
            "name": "generated-expression-%d" % index,
            "source": "int main(void) { int a; int b; a = 1; b = 2; "
                      "a = %s; return a; }\n" % expression,
            "requirements": ["C-001", "C-009"],
        })
    pointer_expressions = [
        ("p = q", "p"), ("p = 0", "p"), ("p == q", "a"),
        ("p != q", "a"), ("p && q", "a"), ("!p", "a"),
        ("p ? q : p", "p"), ("*p", "a"), ("&a", "p"),
        ("p + 1", "p"), ("1 + p", "p"), ("p - q", "a"),
        ("sizeof p", "a"),
    ]
    for index, (expression, target) in enumerate(pointer_expressions):
        cases.append({
            "name": "generated-pointer-%d" % index,
            "source": "int main(void) { int a; int *p; int *q; p = &a; "
                      "q = p; %s = %s; return a; }\n"
                      % (target, expression),
            "requirements": ["C-001", "C-009"],
        })
    statements = [
        "a = a + b;",
        "if (a) a = b;",
        "if (a) a = b; else a = a;",
        "while (a) { a = a - 1; }",
        "{ a = b; }",
        "goto L; L: ;",
        "while (a) { if (b) break; continue; }",
        "a = 1; ; a = b;",
    ]
    for index, statement in enumerate(statements):
        cases.append({
            "name": "generated-statement-%d" % index,
            "source": "int main(void) { int a; int b; a = 0; b = 1; "
                      "%s return a; }\n" % statement,
            "requirements": ["C-005", "C-009"],
        })
    cases.append({
        "name": "generated-parameters",
        "source": "int f(int a, char b, int *c) { return a + b; }\n"
                  "int main(void) { return f(1, 2, 0); }\n",
        "requirements": ["C-004", "C-006", "C-009"],
    })
    cases.append({
        "name": "generated-nested-declarators",
        "source": "int (*f(void))(int);\n"
                  "struct S { int (*g)(int); };\n"
                  "int main(void) { return 0; }\n",
        "requirements": ["C-004", "C-006", "C-009"],
    })
    return cases


# Every alternative of every production in verification/spec/grammar.ebnf.
# A source is an accepted example that exercises the alternative; None marks
# an alternative the current positive corpus does not cover.  The parser
# suite turns each distinct source into a case and records the coverage keys
# it exercises, so a regression such as rejecting "int a, b;" in a struct
# leaves struct-declarator-list/comma explicitly uncovered.
def _main(body):
    return "int main(void) { " + body + " }\n"


GRAMMAR_COVERAGE = [
    ("translation-unit/external-declaration", "int x;\n"),
    ("translation-unit/translation-unit-external-declaration",
     "int x;\nint y;\n"),
    ("external-declaration/function-definition",
     "int f(void) { return 0; }\n"),
    ("external-declaration/declaration", "int x;\n"),
    ("function-definition", "int f(void) { return 0; }\n"),
    ("declaration/with-init-declarator", "int x;\n"),
    ("declaration/without-init-declarator", "struct S;\n"),
    ("declaration-specifiers/type-specifier", "int x;\n"),
    ("type-specifier/void", "void f(void);\n"),
    ("type-specifier/char", "char c;\n"),
    ("type-specifier/int", "int x;\n"),
    ("type-specifier/struct", "struct S;\n"),
    ("struct-or-union-specifier/anonymous", "struct { int x; } s;\n"),
    ("struct-or-union-specifier/tagged-definition", "struct S { int x; };\n"),
    ("struct-or-union-specifier/tag", "struct S;\n"),
    ("struct-declaration-list/single", "struct S { int a; };\n"),
    ("struct-declaration-list/multiple", "struct S { int a; int b; };\n"),
    ("struct-declaration/with-declarator-list", "struct S { int a; };\n"),
    ("struct-declaration/without-declarator-list", None),
    ("struct-declarator-list/single", "struct S { int a; };\n"),
    ("struct-declarator-list/comma", "struct S { int a, b; };\n"),
    ("struct-declarator", "struct S { int a; };\n"),
    ("init-declarator", "int x;\n"),
    ("declarator/without-pointer", "int x;\n"),
    ("declarator/with-pointer", "int *p;\n"),
    ("direct-declarator/identifier", "int x;\n"),
    ("direct-declarator/parenthesized", "int (x);\n"),
    ("direct-declarator/function", "int f(void);\n"),
    ("pointer/single", "int *p;\n"),
    ("pointer/recursive", "int **pp;\n"),
    ("parameter-type-list", "int f(int x);\n"),
    ("parameter-list/single", "int f(int x);\n"),
    ("parameter-list/comma", "int f(int x, char y);\n"),
    ("parameter-declaration/with-declarator", "int f(int x);\n"),
    ("parameter-declaration/without-declarator", "int f(int);\n"),
    ("abstract-declarator/pointer", _main("return sizeof(int *);")),
    ("abstract-declarator/parenthesized", _main("return sizeof(int (*));")),
    ("abstract-declarator/parenthesized-function",
     _main("return sizeof(int (*)(int));")),
    ("direct-abstract-declarator/parenthesized",
     _main("return sizeof(int (*));")),
    ("direct-abstract-declarator/parameter-list", "int f(int (int));\n"),
    ("direct-abstract-declarator/postfix",
     _main("return sizeof(int (*)(int));")),
    ("type-name/without-abstract", _main("return sizeof(int);")),
    ("type-name/with-abstract", _main("return sizeof(int *);")),
    ("primary-expression/identifier", _main("int a; a = 1; return a;")),
    ("primary-expression/constant", _main("return 1;")),
    ("primary-expression/string-literal",
     _main("char *p; p = \"x\"; return 0;")),
    ("primary-expression/parenthesized",
     _main("int a; a = 1; a = (a); return a;")),
    ("postfix-expression/primary", _main("int a; a = 1; return a;")),
    ("postfix-expression/subscript",
     _main("char *p; p = \"x\"; return p[0];")),
    ("postfix-expression/call-without-args",
     "int f(void) { return 0; }\n" + _main("return f();")),
    ("postfix-expression/call-with-args",
     "int f(int x) { return x; }\n" + _main("return f(1);")),
    ("postfix-expression/member",
     "struct S { int x; };\n" + _main("struct S s; s.x = 1; return s.x;")),
    ("postfix-expression/arrow",
     "struct S { int x; };\n" +
     _main("struct S s; struct S *p; p = &s; p->x = 1; return p->x;")),
    ("argument-expression-list/single",
     "int f(int x) { return x; }\n" + _main("return f(1);")),
    ("argument-expression-list/comma",
     "int f(int x, int y) { return x + y; }\n" + _main("return f(1, 2);")),
    ("unary-expression/postfix", _main("int a; a = 1; return a;")),
    ("unary-expression/address", _main("int a; int *p; p = &a; return 0;")),
    ("unary-expression/dereference",
     _main("int a; int *p; p = &a; *p = 1; return *p;")),
    ("unary-expression/minus", _main("return -1;")),
    ("unary-expression/not", _main("return !0;")),
    ("unary-expression/sizeof-expression", _main("int a; return sizeof a;")),
    ("unary-expression/sizeof-type", _main("return sizeof(int);")),
    ("cast-expression", _main("int a; a = 1; return a;")),
    ("multiplicative-expression/base", _main("int a; a = 1; return a;")),
    ("multiplicative-expression/mul", _main("return 2 * 3 * 4;")),
    ("additive-expression/base", _main("int a; a = 1; return a;")),
    ("additive-expression/plus", _main("return 1 + 2 * 3;")),
    ("additive-expression/minus", _main("return 10 - 1 - 2;")),
    ("shift-expression", _main("return 1;")),
    ("relational-expression/base", _main("return 1;")),
    ("relational-expression/less", _main("return 1 < 2 < 3;")),
    ("equality-expression/base", _main("return 1;")),
    ("equality-expression/eq", _main("return 1 == 2 == 0;")),
    ("equality-expression/ne", _main("return 1 != 2;")),
    ("AND-expression", _main("return 1;")),
    ("exclusive-OR-expression", _main("return 1;")),
    ("inclusive-OR-expression", _main("return 1;")),
    ("logical-AND-expression/base", _main("return 1;")),
    ("logical-AND-expression/and", _main("return 1 && 2 && 3;")),
    ("logical-OR-expression/base", _main("return 1;")),
    ("logical-OR-expression/or", _main("return 1 || 2 || 3;")),
    ("conditional-expression/base", _main("return 1;")),
    ("conditional-expression/conditional", _main("return 1 ? 2 : 3 ? 4 : 5;")),
    ("assignment-expression/base", _main("return 1;")),
    ("assignment-expression/assign",
     _main("int a; int b; a = b = 1; return a;")),
    ("expression", _main("return 1;")),
    ("statement/labeled", _main("L: ; return 0;")),
    ("statement/compound", _main("{ } return 0;")),
    ("statement/expression", _main("; return 0;")),
    ("statement/selection", _main("if (1) return 0; return 1;")),
    ("statement/iteration", _main("while (0) { } return 0;")),
    ("statement/jump", _main("return 0;")),
    ("labeled-statement", _main("L: ; return 0;")),
    ("compound-statement/without-block-items", _main("{ } return 0;")),
    ("compound-statement/with-block-items", _main("{ ; } return 0;")),
    ("block-item-list/single", _main("{ ; } return 0;")),
    ("block-item-list/multiple", _main("{ ; ; } return 0;")),
    ("block-item/declaration", _main("{ int x; } return 0;")),
    ("block-item/statement", _main("{ ; } return 0;")),
    ("expression-statement/with-expression", _main("1; return 0;")),
    ("expression-statement/without-expression", _main("; return 0;")),
    ("selection-statement/if", _main("if (1) return 0; return 1;")),
    ("selection-statement/if-else",
     _main("if (1) return 0; else return 1;")),
    ("iteration-statement/while", _main("while (0) { } return 0;")),
    ("jump-statement/goto", _main("goto L; L: ; return 0;")),
    ("jump-statement/continue", _main("while (1) { continue; } return 0;")),
    ("jump-statement/break", _main("while (1) { break; } return 0;")),
    ("jump-statement/return-expression", _main("return 0;")),
    ("jump-statement/return-void",
     "void f(void) { return; }\n" + _main("f(); return 0;")),
]

GRAMMAR_UNCOVERED_NOTES = {
    "struct-declaration/without-declarator-list":
        "No positive C4 example: C11 6.7.2.1p1 requires a struct-declarator-"
        "list unless the declaration declares an anonymous structure or "
        "union, which is outside the C4 grammar; C4 rejects "
        "'struct S { int; };'.",
}


def _grammar_coverage_cases():
    """One positive parser case per distinct grammar-coverage example."""
    by_source = {}
    for key, source in GRAMMAR_COVERAGE:
        if source is not None:
            by_source.setdefault(source, []).append(key)
    cases = []
    for index, (source, keys) in enumerate(by_source.items()):
        cases.append({
            "name": "grammar-%02d-%s"
                    % (index, keys[0].replace("/", "-")),
            "source": source,
            "requirements": ["C-009"],
            "covers": keys,
        })
    return cases


VALID_CASES.extend(_generated_valid_cases())
VALID_CASES.extend(_grammar_coverage_cases())

INVALID_CASES: List[Dict[str, object]] = [
    {
        "name": "empty-translation-unit",
        "source": "",
        "requirements": ["C-008", "A-012"],
    },
    {
        "name": "missing-semicolon",
        "source": "int main(void) { return 0 }\n",
        "requirements": ["C-008", "C-010", "A-012"],
    },
    {
        "name": "missing-expression",
        "source": "int main(void) { int a; a = ; return 0; }\n",
        "requirements": ["C-008", "C-010", "A-012"],
    },
    {
        "name": "missing-close-brace",
        "source": "int main(void) { return 0;\n",
        "requirements": ["C-008", "C-010", "A-012"],
    },
    {
        "name": "declaration-in-if-body",
        "source": "int main(void) { if (1) int x; return 0; }\n",
        "requirements": ["C-005", "C-008", "A-012"],
    },
    {
        "name": "label-before-declaration",
        "source": "int main(void) { label: int x; return 0; }\n",
        "requirements": ["C-005", "C-008", "A-012"],
    },
    {
        "name": "empty-parameter-list",
        "source": "int f() { return 0; }\n",
        "requirements": ["C-004", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "initializer",
        "source": "int x = 1;\n",
        "requirements": ["C-003", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "multiple-declarators",
        "source": "int a, b;\n",
        "requirements": ["C-003", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "for-statement",
        "source": "int main(void) { for (;;) ; return 0; }\n",
        "requirements": ["C-005", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "do-statement",
        "source": "int main(void) { do ; while (1); return 0; }\n",
        "requirements": ["C-005", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "switch-statement",
        "source": "int main(void) { switch (1) { } return 0; }\n",
        "requirements": ["C-005", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "typedef",
        "source": "typedef int T;\n",
        "requirements": ["C-003", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "enum",
        "source": "enum E { A };\n",
        "requirements": ["C-003", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "union",
        "source": "union U { int x; };\n",
        "requirements": ["C-003", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "qualified-type",
        "source": "const int x;\n",
        "requirements": ["C-003", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "unsigned-type",
        "source": "unsigned int x;\n",
        "requirements": ["C-003", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "array-declarator",
        "source": "int x[3];\n",
        "requirements": ["C-004", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "cast-expression",
        "source": "int main(void) { int a; a = (int) 1; return a; }\n",
        "requirements": ["C-002", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "division",
        "source": "int main(void) { int a; int b; a = a / b; return a; }\n",
        "requirements": ["C-002", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "less-equal",
        "source": "int main(void) { int a; int b; a = a <= b; return a; }\n",
        "requirements": ["C-002", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "shift",
        "source": "int main(void) { int a; a = a << 1; return a; }\n",
        "requirements": ["C-002", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "bitwise-not",
        "source": "int main(void) { int a; a = ~a; return a; }\n",
        "requirements": ["C-002", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "increment",
        "source": "int main(void) { int a; a = 1; a++; return a; }\n",
        "requirements": ["C-002", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "compound-assignment",
        "source": "int main(void) { int a; a = 1; a += 1; return a; }\n",
        "requirements": ["C-002", "A-011"],
        "kind": "may-reject",
    },
    {
        "name": "comma-expression",
        "source": "int main(void) { int a; a = 1; a = (a, a); return a; }\n",
        "requirements": ["C-002", "A-011"],
        "kind": "may-reject",
    },
]

MUTATION_BASES = [
    "int main(void) { return 0; }\n",
    "int f(int x) { if (x < 2) return x; return f(x - 1); }\n",
    "struct S { int a; }; int main(void) { struct S s; s.a = 1; return s.a; }\n",
    "int main(void) { int a; int b; a = 1; b = 2; while (a < b) { a = a + 1; } return a; }\n",
    "int main(void) { int *p; int a; p = &a; *p = 3; return *p; }\n",
    "void g(void) { } int main(void) { g(); return 0; }\n",
    "int main(void) { int a; a = 1 ? 2 : 3; return a; }\n",
    "int main(void) { int a; a = sizeof(int); return a; }\n",
]


def _reference_accepts(source: str) -> bool:
    from c4ref.lexer import lex
    from c4ref.parser import parse
    from c4ref.sema import check
    try:
        return check(parse(lex(source))).accepted
    except Exception:
        return False


def _reference_error(source: str):
    from c4ref.lexer import lex
    from c4ref.parser import parse
    from c4ref.sema import check
    try:
        check(parse(lex(source)))
    except Exception as exc:
        return exc
    return None


def _reference_rejects(source: str) -> bool:
    return not _reference_accepts(source)


def _c_valid(ctx: Context, source: str) -> bool:
    """Use the host C compiler only as a C-validity oracle.

    This distinguishes invalid C (which must be rejected) from valid C
    outside the C4 grammar (which may be rejected).  It is not used to
    define C4 semantics.
    """
    if ctx.clang is None:
        return False
    work = ctx.tempdir("parse-cvalid-")
    try:
        ctx.write_source(work / "case.c", source)
        proc = ctx.run([ctx.clang, "-std=c11", "-pedantic-errors",
                        "-Wno-strict-prototypes",
                        "-fsyntax-only", "case.c"], cwd=work, timeout=60)
        return proc.returncode == 0
    finally:
        ctx.cleanup(work)


def _diagnostic(proc):
    matches = [DIAGNOSTIC_RE.match(line) for line in proc.stderr.splitlines()]
    return [m for m in matches if m]


def _report_reject(ctx, reporter, name, requirements, source, kind):
    work = ctx.tempdir("parse-reject-")
    try:
        ctx.write_source(work / "case.c", source)
        proc = ctx.c4run("--parse", "case.c", work)
        checks = Checks()
        if kind == "may-reject":
            # Rejection is conforming; acceptance is recorded as an
            # implementation extension and is not a conformance failure.
            if proc.returncode == 0:
                reporter.record("parser", name, requirements, True,
                                "accepted as an extension outside C4")
                return
        checks.check(proc.returncode == 1,
                     "exit %d, expected 1" % proc.returncode)
        matches = _diagnostic(proc)
        checks.check(bool(matches), "missing diagnostic: %r"
                     % proc.stderr[:200])
        if kind != "may-reject" and matches:
            reference_error = _reference_error(source)
            location = getattr(reference_error, "loc", None)
            if location is not None:
                match = matches[0]
                checks.check(
                    (int(match.group("line")),
                     int(match.group("column"))) ==
                    (location.line, location.column),
                    "diagnostic at %s:%s, reference %d:%d"
                    % (match.group("line"), match.group("column"),
                       location.line, location.column))
        reporter.record("parser", name, requirements, checks.ok,
                        checks.detail())
    finally:
        ctx.cleanup(work)


def run_parser(ctx: Context, reporter: Reporter):
    covered = set()
    for case in VALID_CASES:
        name = str(case["name"])
        source = str(case["source"])
        requirements = list(case["requirements"])
        work = ctx.tempdir("parse-valid-")
        try:
            ctx.write_source(work / "case.c", source)
            checks = Checks()
            checks.check(_reference_accepts(source),
                         "reference frontend rejected a valid case")
            proc = ctx.c4run("--parse", "case.c", work)
            checks.check(proc.returncode == 0,
                         "c4 exited %d: %s"
                         % (proc.returncode, proc.stderr.strip()[:200]))
            passed = checks.ok
            reporter.record("parser", name, requirements, passed,
                            checks.detail())
            if passed:
                covered.update(case.get("covers", []))
        finally:
            ctx.cleanup(work)

    for case in INVALID_CASES:
        kind = str(case.get("kind", "invalid"))
        requirements = list(case["requirements"])
        source = str(case["source"])
        if kind != "may-reject":
            # Invalid C: the reference frontend must reject it too.
            if not _reference_rejects(source):
                reporter.check("parser", str(case["name"]), requirements,
                               False, "reference frontend accepted invalid C")
                continue
            if _c_valid(ctx, source):
                reporter.check("parser", str(case["name"]), requirements,
                               False, "case classified invalid but clang "
                               "accepts it as C")
                continue
        _report_reject(ctx, reporter, str(case["name"]), requirements,
                       source, kind)

    # Systematic invalid inputs: delete one token from a valid program and
    # keep only the mutants the reference frontend rejects.
    mutants = 0
    for base_index, base in enumerate(MUTATION_BASES):
        tokens = reference_tokens(base)
        for index in range(min(len(tokens), 30)):
            mutant_tokens = tokens[:index] + tokens[index + 1:]
            source = " ".join(token[3] for token in mutant_tokens) + "\n"
            if _reference_accepts(source):
                continue
            # A reference rejection only proves that the mutant is outside
            # C4.  It is a required rejection only when the mutant is not C.
            valid_c = _c_valid(ctx, source)
            mutants += 1
            name = "mutant-%d-%d" % (base_index, index)
            work = ctx.tempdir("parse-mutant-")
            try:
                ctx.write_source(work / "case.c", source)
                proc = ctx.c4run("--parse", "case.c", work)
                matches = _diagnostic(proc)
                if valid_c:
                    ok = proc.returncode == 0 or (proc.returncode == 1 and
                                                  bool(matches))
                else:
                    ok = proc.returncode == 1 and bool(matches)
                reporter.record(
                    "parser", name,
                    ["C-008", "A-012"] if not valid_c else ["C-008", "A-011"],
                    ok,
                    "" if ok else "exit %d, stderr %r"
                    % (proc.returncode, proc.stderr[:160]))
            finally:
                ctx.cleanup(work)
    print("[parser] %d deletion mutants rejected by both frontends" % mutants)

    alternatives = [key for key, _ in GRAMMAR_COVERAGE]
    uncovered = sorted(set(alternatives) - covered)
    reporter.metadata["grammar_coverage"] = {
        "alternatives": len(alternatives),
        "covered": len(alternatives) - len(uncovered),
        "uncovered": uncovered,
        "uncovered_notes": {
            key: GRAMMAR_UNCOVERED_NOTES[key]
            for key in uncovered if key in GRAMMAR_UNCOVERED_NOTES
        },
    }
    print("[parser] grammar coverage: %d/%d alternatives covered"
          % (len(alternatives) - len(uncovered), len(alternatives)))
    if uncovered:
        print("[parser] uncovered alternatives: " + ", ".join(uncovered))

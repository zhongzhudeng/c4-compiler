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
            reporter.record("parser", name, requirements, checks.ok,
                            checks.detail())
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

"""Sema conformance against the independent reference checker.

Each case is a grammar-valid C4 program.  The reference checker decides
accept/reject/exempt; c4 --parse must agree, except that an exempted program
may be accepted or rejected (Task E exemption).  Error locations are compared
exactly for the categories where Task E states an unambiguous rule.
"""

from __future__ import annotations

from typing import Dict, List

from .runner import Checks, Context, Reporter, DIAGNOSTIC_RE


CASES: List[Dict[str, object]] = [
    # -- S-001/S-003 scopes and shadowing -----------------------------
    {"name": "file-var-visible", "expect": "accept",
     "source": "int x;\nint main(void) { return x; }\n",
     "requirements": ["S-001"]},
    {"name": "block-shadow", "expect": "accept",
     "source": "int x;\nint main(void) { int x; x = 1; return x; }\n",
     "requirements": ["S-001", "S-003"]},
    {"name": "outer-restored", "expect": "accept",
     "source": "int x;\nint main(void) { x = 1; { int x; x = 2; } return x; }\n",
     "requirements": ["S-001", "S-003"]},
    {"name": "use-before-declaration", "expect": "reject",
     "source": "int main(void) { x = 1; int x; return x; }\n",
     "requirements": ["S-001"]},
    {"name": "prototype-param-scope", "expect": "reject",
     "source": "int f(int x);\nint main(void) { return x; }\n",
     "requirements": ["S-001"]},
    {"name": "label-function-scope", "expect": "accept",
     "source": "int main(void) { goto L; { L: ; } return 0; }\n",
     "requirements": ["S-001", "S-022"]},
    {"name": "block-function-declaration", "expect": "accept",
     "source": "int f(void) { return 1; }\n"
               "int main(void) { int f(void); return f(); }\n",
     "requirements": ["S-001", "S-004"]},

    # -- S-002 namespaces ---------------------------------------------
    {"name": "tag-vs-ordinary", "expect": "accept",
     "source": "struct S { int a; };\nint S;\n"
               "int main(void) { S = 1; return S; }\n",
     "requirements": ["S-002"]},
    {"name": "label-vs-ordinary", "expect": "accept",
     "source": "int main(void) { int L; L = 1; L: ; return L; }\n",
     "requirements": ["S-002"]},
    {"name": "member-vs-ordinary", "expect": "accept",
     "source": "struct S { int x; };\nint x;\n"
               "int main(void) { struct S s; s.x = x; return s.x; }\n",
     "requirements": ["S-002"]},

    # -- S-004/S-005 linkage, redeclarations, tentative definitions ---
    {"name": "compatible-function-redecl", "expect": "accept",
     "source": "int f(int x);\nint f(int x) { return x; }\n"
               "int main(void) { return f(1); }\n",
     "requirements": ["S-005"]},
    {"name": "incompatible-function-redecl", "expect": "reject",
     "source": "int f(int x);\nchar f(int x) { return 0; }\n",
     "requirements": ["S-005"]},
    {"name": "incompatible-object-redecl", "expect": "reject",
     "source": "int x;\nchar x;\n",
     "requirements": ["S-005"]},
    {"name": "block-redeclaration", "expect": "reject",
     "source": "int main(void) { int x; int x; return 0; }\n",
     "requirements": ["S-005"]},
    {"name": "type-equality-function", "expect": "accept",
     "source": "int f(int x);\nint f(int x) { return x; }\n",
     "requirements": ["S-012"]},
    {"name": "type-equality-pointer", "expect": "accept",
     "source": "int main(void) { int *p; int *q; p = q; return 0; }\n",
     "requirements": ["S-012"]},
    {"name": "type-inequality-pointer", "expect": "reject",
     "source": "int main(void) { int *p; char *q; p = q; return 0; }\n",
     "requirements": ["S-012"]},
    {"name": "type-equality-function-pointer", "expect": "accept",
     "source": "int f(int x) { return x; }\n"
               "int main(void) { int (*p)(int); p = f; return p(1); }\n",
     "requirements": ["S-012"]},
    {"name": "tentative-coalesce", "expect": "accept",
     "source": "int x;\nint x;\nint main(void) { return x; }\n",
     "requirements": ["S-006"]},
    {"name": "tentative-incomplete", "expect": "reject",
     "source": "struct S;\nstruct S g;\n",
     "requirements": ["S-006", "S-008"]},

    # -- S-007/S-008 tags and completeness ----------------------------
    {"name": "tag-shadow", "expect": "reject", "check_location": True,
     "source": "struct S { int a; };\n"
               "int main(void) {\n"
               "\tstruct S *p;\n"
               "\t{\n"
               "\t\tstruct S;\n"
               "\t\tstruct S *q;\n"
               "\t\tp = q;\n"
               "\t}\n"
               "\treturn 0;\n"
               "}\n",
     "requirements": ["S-007", "S-008"]},
    {"name": "same-tag-redefinition", "expect": "reject",
     "source": "struct S { int a; };\nstruct S { int b; };\n",
     "requirements": ["S-007", "S-008"]},
    {"name": "forward-then-definition", "expect": "accept",
     "source": "struct S;\nstruct S { int a; };\n",
     "requirements": ["S-007", "S-008"]},
    {"name": "distinct-tag-pointer", "expect": "reject",
     "source": "struct S { int a; };\nstruct T { int a; };\n"
               "int main(void) { struct S s; struct T t; struct S *p; "
               "p = &t; return 0; }\n",
     "requirements": ["S-007"]},
    {"name": "distinct-untagged-pointer", "expect": "reject",
     "source": "int main(void) {\n"
               "\tstruct { int a; } x;\n"
               "\tstruct { int a; } y;\n"
               "\tstruct { int a; } *p;\n"
               "\tp = &x; p = &y;\n"
               "\treturn 0;\n"
               "}\n",
     "requirements": ["S-007"]},
    {"name": "incomplete-object", "expect": "reject",
     "source": "struct S;\nstruct S s;\n",
     "requirements": ["S-008"]},
    {"name": "self-pointer", "expect": "accept",
     "source": "struct S { struct S *next; };\n",
     "requirements": ["S-008"]},
    {"name": "self-value", "expect": "reject",
     "source": "struct S { struct S s; };\n",
     "requirements": ["S-008"]},
    {"name": "duplicate-member", "expect": "reject",
     "source": "struct S { int a; int a; };\n",
     "requirements": ["S-008"]},
    # -- Assignment C/E findings: struct declarators and prototype scope --
    {"name": "comma-struct-members", "expect": "accept",
     "source": "struct S { int a, b; };\n"
               "int main(void) { struct S s; s.a = 20; s.b = 22; "
               "return s.a + s.b; }\n",
     "requirements": ["C-003", "S-008"]},
    {"name": "comma-struct-different-shapes", "expect": "accept",
     "source": "struct S { int *p, q; };\n"
               "int main(void) { struct S s; int x; s.p = &x; s.q = 2; "
               "return s.q; }\n",
     "requirements": ["C-004", "S-009"]},
    {"name": "comma-struct-duplicate-member", "expect": "reject",
     "source": "struct S { int a, a; };\n",
     "requirements": ["S-008"]},
    {"name": "bare-untagged-struct", "expect": "reject",
     "source": "struct { int x; };\nint main(void) { return 0; }\n",
     "requirements": ["C-003", "S-008"]},
    {"name": "no-declarator-member", "expect": "reject",
     "source": "struct S { int; };\n",
     "requirements": ["S-008"]},
    {"name": "tag-only-member", "expect": "reject",
     "source": "struct S { struct T; };\n",
     "requirements": ["S-008"]},
    {"name": "tag-forward-valid", "expect": "accept",
     "source": "struct S;\nint main(void) { return 0; }\n",
     "requirements": ["C-003", "S-007"]},
    {"name": "tag-definition-valid", "expect": "accept",
     "source": "struct S { int x; };\nint main(void) { return 0; }\n",
     "requirements": ["C-003", "S-007"]},
    {"name": "untagged-struct-declarator-valid", "expect": "accept",
     "source": "struct { int x; } s;\n"
               "int main(void) { s.x = 1; return s.x; }\n",
     "requirements": ["C-003", "S-008"]},
    {"name": "prototype-tag-scope-leak", "expect": "reject",
     "source": "int f(struct S *p);\nstruct S { int x; };\n"
               "int f(struct S *p) { return 0; }\n",
     "requirements": ["S-001", "S-005", "S-007"]},
    {"name": "prototype-tag-visible", "expect": "accept",
     "source": "struct S;\nint f(struct S *p);\nstruct S { int x; };\n"
               "int f(struct S *p) { return 0; }\n",
     "requirements": ["S-001", "S-005", "S-007"]},
    {"name": "prototype-tag-repeated", "expect": "reject",
     "source": "int f(struct S *p);\nint f(struct S *p);\n",
     "requirements": ["S-005", "S-007"]},
    {"name": "prototype-tag-definition-visible", "expect": "accept",
     "source": "int f(struct S *p) { struct S *q; return 0; }\n",
     "requirements": ["S-001", "S-007"]},
    {"name": "nested-prototype-tag-scope", "expect": "reject",
     "source": "int f(int (*g)(struct S *), struct S *p);\n"
               "struct S { int x; };\n"
               "int f(int (*g)(struct S *), struct S *p) { return 0; }\n",
     "requirements": ["S-001", "S-005", "S-007"]},
    {"name": "nested-prototype-tag-visible", "expect": "accept",
     "source": "struct S;\n"
               "int f(int (*g)(struct S *), struct S *p);\n"
               "struct S { int x; };\n"
               "int f(int (*g)(struct S *), struct S *p) { return 0; }\n",
     "requirements": ["S-001", "S-005", "S-007"]},
    {"name": "comma-struct-shared-nested-spec", "expect": "accept",
     "source": "struct S { struct T { int x; } a, b; };\n"
               "int main(void) { struct S s; s.a.x = 1; s.b.x = 2; "
               "return s.a.x + s.b.x; }\n",
     "requirements": ["C-003", "S-008"]},
    {"name": "nested-function-definition-tag-visible", "expect": "accept",
     "source": "int (*f(struct S *p))(int) { struct S *q; q = p; "
               "return 0; }\n",
     "requirements": ["S-001", "S-007", "S-009"]},
    {"name": "nested-return-tag-hidden", "expect": "reject",
     "source": "int (*f(struct S *p))(struct T { int x; } *) { "
               "struct T *r; r->x = 1; return 0; }\n",
     "requirements": ["S-001", "S-007", "S-009"]},
    {"name": "nested-parameter-tag-hidden", "expect": "reject",
     "source": "int f(int g(struct T { int x; } *p)) { "
               "struct T *r; r->x = 1; return 0; }\n",
     "requirements": ["S-001", "S-007", "S-010"]},
    {"name": "member-incomplete", "expect": "reject",
     "source": "struct S;\nstruct T { struct S s; };\n",
     "requirements": ["S-008"]},

    # -- S-009/S-010 declarators and parameters -----------------------
    {"name": "function-returning-pointer", "expect": "accept",
     "source": "int *f(void) { return 0; }\n",
     "requirements": ["S-009"]},
    {"name": "pointer-to-function", "expect": "accept",
     "source": "int f(int x) { return x; }\n"
               "int main(void) { int (*p)(int); p = f; return p(1); }\n",
     "requirements": ["S-009"]},
    {"name": "function-returning-function", "expect": "reject",
     "source": "int f(void)(void);\n",
     "requirements": ["S-009"]},
    {"name": "parenthesized-declarator", "expect": "accept",
     "source": "int (f)(void) { return 0; }\n",
     "requirements": ["S-009"]},
    {"name": "parameter-function-adjusted", "expect": "accept",
     "source": "int f(int g(void));\n",
     "requirements": ["S-009", "S-010"]},
    {"name": "definition-param-name-required", "expect": "reject",
     "source": "int f(int) { return 0; }\n",
     "requirements": ["S-010"]},
    {"name": "void-only-parameter", "expect": "reject",
     "source": "int f(void, int x) { return 0; }\n",
     "requirements": ["S-010"]},
    {"name": "incomplete-parameter", "expect": "exempt",
     "source": "struct S;\nint f(struct S s) { return 0; }\n",
     "requirements": ["S-010", "S-029", "A-013"]},
    {"name": "duplicate-parameter-prototype", "expect": "reject",
     "source": "int f(int a, int a);\n",
     "requirements": ["S-010"]},
    {"name": "duplicate-parameter-definition", "expect": "reject",
     "source": "int f(int a, int a) { return 0; }\n",
     "requirements": ["S-010"]},
    {"name": "duplicate-parameter-unnamed", "expect": "accept",
     "source": "int f(int, int);\n",
     "requirements": ["S-010"]},

    # -- S-011..S-014 types, conversions, null pointer ----------------
    {"name": "char-to-int", "expect": "accept",
     "source": "int main(void) { char c; int i; c = 65; i = c; return i; }\n",
     "requirements": ["S-011", "S-013"]},
    {"name": "int-to-char", "expect": "accept",
     "source": "int main(void) { char c; c = 300; return c; }\n",
     "requirements": ["S-011", "S-013"]},
    {"name": "pointer-to-int", "expect": "reject",
     "source": "int main(void) { int *p; int i; i = p; return i; }\n",
     "requirements": ["S-013", "S-020"]},
    {"name": "int-to-pointer", "expect": "reject",
     "source": "int main(void) { int *p; p = 1; return 0; }\n",
     "requirements": ["S-013", "S-014", "S-020"]},
    {"name": "null-literal", "expect": "accept",
     "source": "int main(void) { int *p; p = 0; return 0; }\n",
     "requirements": ["S-014"]},
    {"name": "null-parenthesized", "expect": "either",
     "source": "int main(void) { int *p; p = (0); return 0; }\n",
     "requirements": ["S-014"]},
    {"name": "string-decay", "expect": "accept",
     "source": "int main(void) { char *p; p = \"abc\"; return 0; }\n",
     "requirements": ["S-011", "S-013"]},
    {"name": "function-decay", "expect": "accept",
     "source": "int f(void) { return 0; }\n"
               "int main(void) { int (*p)(void); p = f; return 0; }\n",
     "requirements": ["S-011", "S-013"]},
    {"name": "void-value-used", "expect": "reject",
     "source": "void g(void) { }\n"
               "int main(void) { int x; x = g(); return x; }\n",
     "requirements": ["S-011", "S-013"]},
    {"name": "struct-not-scalar", "expect": "reject",
     "source": "struct S { int a; };\n"
               "int main(void) { struct S s; if (s) return 1; return 0; }\n",
     "requirements": ["S-011", "S-021"]},

    # -- S-015 primary and postfix expressions ------------------------
    {"name": "undeclared-identifier", "expect": "reject",
     "source": "int main(void) { return missing; }\n",
     "requirements": ["S-015"]},
    {"name": "parenthesized-lvalue", "expect": "accept",
     "source": "int main(void) { int x; (x) = 1; return x; }\n",
     "requirements": ["S-015"]},
    {"name": "subscript-pointer", "expect": "accept",
     "source": "int main(void) { int *p; int a; p = &a; return p[0]; }\n",
     "requirements": ["S-016"]},
    {"name": "subscript-integer", "expect": "reject",
     "source": "int main(void) { int a; return a[0]; }\n",
     "requirements": ["S-016"]},
    {"name": "call-arity", "expect": "reject",
     "source": "int f(int x) { return x; }\n"
               "int main(void) { return f(); }\n",
     "requirements": ["S-016", "S-026"]},
    {"name": "call-argument-type", "expect": "reject",
     "source": "int f(int *p) { return 0; }\n"
               "int main(void) { return f(1); }\n",
     "requirements": ["S-016", "S-026"]},
    {"name": "member-missing", "expect": "reject",
     "source": "struct S { int a; };\n"
               "int main(void) { struct S s; return s.b; }\n",
     "requirements": ["S-016", "S-027"]},
    {"name": "arrow-on-struct", "expect": "reject",
     "source": "struct S { int a; };\n"
               "int main(void) { struct S s; return s->a; }\n",
     "requirements": ["S-016", "S-027"]},

    # -- S-017 unary expressions --------------------------------------
    {"name": "address-of-rvalue", "expect": "reject",
     "source": "int main(void) { int *p; p = &1; return 0; }\n",
     "requirements": ["S-017"]},
    {"name": "deref-nonpointer", "expect": "reject",
     "source": "int main(void) { int x; return *x; }\n",
     "requirements": ["S-017"]},
    {"name": "negate-pointer", "expect": "reject",
     "source": "int main(void) { int *p; return -p; }\n",
     "requirements": ["S-017"]},
    {"name": "not-struct", "expect": "reject",
     "source": "struct S { int a; };\n"
               "int main(void) { struct S s; return !s; }\n",
     "requirements": ["S-017"]},
    {"name": "sizeof-function", "expect": "reject",
     "source": "int f(void) { return 0; }\n"
               "int main(void) { return sizeof f; }\n",
     "requirements": ["S-017"]},
    {"name": "sizeof-incomplete", "expect": "reject",
     "source": "struct S;\nint main(void) { return sizeof(struct S); }\n",
     "requirements": ["S-017"]},
    {"name": "sizeof-result-int", "expect": "reject",
     "source": "int main(void) { int *p; p = sizeof(int); return 0; }\n",
     "requirements": ["S-017"]},

    # -- S-018 binary operators ---------------------------------------
    {"name": "pointer-plus-int", "expect": "accept",
     "source": "int main(void) { int *p; p = p + 1; return 0; }\n",
     "requirements": ["S-018"]},
    {"name": "pointer-plus-pointer", "expect": "reject",
     "source": "int main(void) { int *p; int *q; p = p + q; return 0; }\n",
     "requirements": ["S-018"]},
    {"name": "multiply-pointer", "expect": "reject",
     "source": "int main(void) { int *p; p = p * 2; return 0; }\n",
     "requirements": ["S-018"]},
    {"name": "compare-pointer-int", "expect": "reject",
     "source": "int main(void) { int *p; int a; a = p == 1; return a; }\n",
     "requirements": ["S-018"]},
    {"name": "logical-pointer", "expect": "accept",
     "source": "int main(void) { int *p; int a; a = p && 1; return a; }\n",
     "requirements": ["S-018"]},
    {"name": "pointer-subtract", "expect": "accept",
     "source": "int main(void) { int *p; int *q; int a; a = p - q; "
               "return a; }\n",
     "requirements": ["S-018"]},
    {"name": "compare-incompatible-pointers", "expect": "reject",
     "source": "int main(void) { int *p; char *q; int a; a = p == q; "
               "return a; }\n",
     "requirements": ["S-018"]},

    # -- S-019 conditional --------------------------------------------
    {"name": "conditional-int", "expect": "accept",
     "source": "int main(void) { int a; a = 1 ? 2 : 3; return a; }\n",
     "requirements": ["S-019"]},
    {"name": "conditional-pointer-null", "expect": "accept",
     "source": "int main(void) { int *p; int *q; q = 1 ? p : 0; "
               "return 0; }\n",
     "requirements": ["S-019"]},
    {"name": "conditional-pointer-int", "expect": "reject",
     "source": "int main(void) { int *p; int *q; q = 1 ? p : 2; "
               "return 0; }\n",
     "requirements": ["S-019"]},
    {"name": "conditional-void", "expect": "accept",
     "source": "void g(void) { }\n"
               "int main(void) { int a; a = 1; a ? g() : g(); return 0; }\n",
     "requirements": ["S-019"]},
    {"name": "conditional-not-lvalue", "expect": "reject",
     "source": "int main(void) { int x; int y; (1 ? x : y) = 2; "
               "return 0; }\n",
     "requirements": ["S-019"]},

    # -- S-020 assignment ---------------------------------------------
    {"name": "assign-to-rvalue", "expect": "reject",
     "source": "int main(void) { 1 = 2; return 0; }\n",
     "requirements": ["S-020"]},
    {"name": "assign-to-expression", "expect": "reject",
     "source": "int main(void) { int x; (x + 1) = 2; return 0; }\n",
     "requirements": ["S-020"]},
    {"name": "assignment-result-type", "expect": "accept",
     "source": "int main(void) { char c; int i; i = (c = 65); return i; }\n",
     "requirements": ["S-020"]},

    # -- S-021..S-024 statements, labels, jumps, return ---------------
    {"name": "if-pointer-condition", "expect": "accept",
     "source": "int main(void) { int *p; if (p) return 1; return 0; }\n",
     "requirements": ["S-021"]},
    {"name": "while-pointer-condition", "expect": "accept",
     "source": "int main(void) { int *p; while (p) return 1; return 0; }\n",
     "requirements": ["S-021"]},
    {"name": "while-struct-condition", "expect": "reject",
     "source": "struct S { int a; };\n"
               "int main(void) { struct S s; while (s) return 1; "
               "return 0; }\n",
     "requirements": ["S-021"]},
    {"name": "goto-missing-label", "expect": "reject",
     "source": "int main(void) { goto nowhere; return 0; }\n",
     "requirements": ["S-022"]},
    {"name": "duplicate-label", "expect": "reject",
     "source": "int main(void) { L: ; L: ; return 0; }\n",
     "requirements": ["S-022"]},
    {"name": "break-outside-loop", "expect": "reject",
     "source": "int main(void) { break; return 0; }\n",
     "requirements": ["S-023"]},
    {"name": "continue-outside-loop", "expect": "reject",
     "source": "int main(void) { continue; return 0; }\n",
     "requirements": ["S-023"]},
    {"name": "break-inside-loop", "expect": "accept",
     "source": "int main(void) { while (1) { break; } return 0; }\n",
     "requirements": ["S-023"]},
    {"name": "continue-inside-loop", "expect": "accept",
     "source": "int main(void) { while (1) { continue; } return 0; }\n",
     "requirements": ["S-023"]},
    {"name": "return-value-in-void", "expect": "reject",
     "source": "void f(void) { return 1; }\n",
     "requirements": ["S-024"]},
    {"name": "bare-return-in-nonvoid", "expect": "reject",
     "source": "int f(void) { return; }\n",
     "requirements": ["S-024"]},
    {"name": "bare-return-in-void", "expect": "accept",
     "source": "void f(void) { return; }\n",
     "requirements": ["S-024"]},
    {"name": "return-null-pointer", "expect": "accept",
     "source": "int *f(void) { return 0; }\n",
     "requirements": ["S-024", "S-014"]},
    {"name": "return-int-as-pointer", "expect": "reject",
     "source": "int *f(void) { return 1; }\n",
     "requirements": ["S-024"]},

    # -- S-025 function definitions -----------------------------------
    {"name": "definition-not-a-function", "expect": "reject",
     "source": "int x { return 0; }\n",
     "requirements": ["S-025"]},
    {"name": "incomplete-return-type", "expect": "exempt",
     "source": "struct S;\nstruct S f(void) { return 0; }\n",
     "requirements": ["S-025", "S-029", "A-013"]},

    # -- S-026 calls ---------------------------------------------------
    {"name": "call-nonfunction", "expect": "reject",
     "source": "int main(void) { int x; return x(); }\n",
     "requirements": ["S-026"]},
    {"name": "call-recursive", "expect": "accept",
     "source": "int f(int n) { if (n < 2) return n; "
               "return f(n - 1) + f(n - 2); }\n"
               "int main(void) { return f(5); }\n",
     "requirements": ["S-026"]},

    # -- S-027 members -------------------------------------------------
    {"name": "member-lvalue", "expect": "accept",
     "source": "struct S { int a; };\n"
               "int main(void) { struct S s; s.a = 1; return s.a; }\n",
     "requirements": ["S-027"]},
    {"name": "arrow-member-lvalue", "expect": "accept",
     "source": "struct S { int a; };\n"
               "int main(void) { struct S s; struct S *p; p = &s; "
               "p->a = 1; return s.a; }\n",
     "requirements": ["S-027"]},
    {"name": "member-of-nonstruct", "expect": "reject",
     "source": "int main(void) { int x; return x.a; }\n",
     "requirements": ["S-027"]},

    # -- S-028 error locations ----------------------------------------
    {"name": "location-binary-operator", "expect": "reject",
     "check_location": True,
     "source": "int main(void) { int *p; int *q; p = p + q; return 0; }\n",
     "requirements": ["S-028", "A-007"]},
    {"name": "location-if-keyword", "expect": "reject",
     "check_location": True,
     "source": "struct S { int a; };\n"
               "int main(void) { struct S s; if (s) return 1; return 0; }\n",
     "requirements": ["S-028", "A-007"]},
    {"name": "location-assignment", "expect": "reject",
     "check_location": True,
     "source": "int main(void) { int *p; int x; x = p; return 0; }\n",
     "requirements": ["S-028", "A-007"]},

    # -- S-029 exemptions ---------------------------------------------
    {"name": "exempt-struct-assignment", "expect": "exempt",
     "source": "struct S { int a; };\n"
               "int main(void) { struct S x; struct S y; x = y; "
               "return 0; }\n",
     "requirements": ["S-029", "A-013"]},
    {"name": "exempt-struct-parameter", "expect": "exempt",
     "source": "struct S { int a; };\n"
               "int f(struct S s) { return 0; }\n",
     "requirements": ["S-029", "A-013"]},
    {"name": "exempt-struct-return", "expect": "exempt",
     "source": "struct S { int a; };\n"
               "struct S f(void) { struct S s; return s; }\n",
     "requirements": ["S-029", "A-013"]},
    {"name": "exempt-anonymous-member", "expect": "exempt",
     "source": "struct S { struct { int x; }; };\n",
     "requirements": ["S-029", "A-013"]},
    {"name": "exempt-func-name", "expect": "exempt",
     "source": "int main(void) { char *p; p = __func__; return 0; }\n",
     "requirements": ["S-029", "A-013"]},

    # -- S-030 no expression evaluation -------------------------------
    {"name": "no-evaluation", "expect": "accept",
     "source": "int main(void) { int x; x = 1 + 2; return x; }\n",
     "requirements": ["S-030"]},
    {"name": "no-overflow-evaluation", "expect": "accept",
     "source": "int main(void) { int x; x = 2147483647 + 1; return x; }\n",
     "requirements": ["S-030"]},
]


def _reference_outcome(source: str):
    from c4ref.lexer import lex
    from c4ref.parser import parse
    from c4ref.sema import check
    try:
        result = check(parse(lex(source)))
    except Exception as exc:
        return "parse-error", exc
    if result.exempted:
        return "exempt", result.error
    if result.accepted:
        return "accept", None
    return "reject", result.error


def run_sema(ctx: Context, reporter: Reporter):
    for case in CASES:
        name = str(case["name"])
        source = str(case["source"])
        expected = str(case["expect"])
        requirements = list(case["requirements"])
        outcome, error = _reference_outcome(source)
        if outcome != expected and not (expected == "either" and
                                        outcome == "reject"):
            reporter.check(
                "sema", name, requirements, False,
                "case expects %s but reference says %s (%s)"
                % (expected, outcome,
                   getattr(error, "message", error)))
            continue

        work = ctx.tempdir("sema-")
        try:
            ctx.write_source(work / "case.c", source)
            proc = ctx.c4run("--parse", "case.c", work)
            if expected == "accept":
                reporter.check(
                    "sema", name, requirements, proc.returncode == 0,
                    "c4 exited %d: %s"
                    % (proc.returncode, proc.stderr.strip()[:200]))
                continue

            if expected == "exempt":
                if proc.returncode == 0:
                    reporter.record("sema", name, requirements, True,
                                    "accepted outside the required domain")
                else:
                    matches = [DIAGNOSTIC_RE.match(line)
                               for line in proc.stderr.splitlines()]
                    matches = [m for m in matches if m]
                    reporter.check(
                        "sema", name, requirements,
                        proc.returncode == 1 and bool(matches),
                        "exempted program rejected with exit %d and "
                        "stderr %r" % (proc.returncode, proc.stderr[:160]))
                continue

            if expected == "either":
                # The reference enforces the required domain; c4 may support
                # the valid-C extension, in which case acceptance is allowed.
                if proc.returncode == 0:
                    reporter.record("sema", name, requirements, True,
                                    "accepted as a valid-C extension")
                    continue
                matches = [DIAGNOSTIC_RE.match(line)
                           for line in proc.stderr.splitlines()]
                matches = [m for m in matches if m]
                reporter.check(
                    "sema", name, requirements,
                    proc.returncode == 1 and bool(matches),
                    "exit %d, stderr %r"
                    % (proc.returncode, proc.stderr[:160]))
                continue

            # Required rejection.
            checks = Checks()
            checks.check(proc.returncode == 1,
                         "exit %d, expected 1" % proc.returncode)
            matches = [DIAGNOSTIC_RE.match(line)
                       for line in proc.stderr.splitlines()]
            matches = [m for m in matches if m]
            checks.check(bool(matches), "missing diagnostic: %r"
                         % proc.stderr[:200])
            if case.get("check_location") and matches and error is not None:
                match = matches[0]
                checks.check(
                    (int(match.group("line")), int(match.group("column"))) ==
                    (error.loc.line, error.loc.column),
                    "diagnostic at %s:%s, reference %d:%d (%s)"
                    % (match.group("line"), match.group("column"),
                       error.loc.line, error.loc.column, error.message))
            reporter.record("sema", name, requirements, checks.ok,
                            checks.detail())
        finally:
            ctx.cleanup(work)

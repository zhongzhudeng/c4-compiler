"""Printer conformance: exact reference output, normalization, round trips.

The reference parser is used to compare the structure before and after
printing (D-003, D-007) and to check full parenthesization (D-005).  The
reference pretty file is compared byte-for-byte (D-002).
"""

from __future__ import annotations

from pathlib import Path
from typing import Dict, List

from .runner import (Checks, Context, Reporter, ast_equal, reference_tokens)

ATOMS = {"identifier", "constant", "string"}
DIGRAPHS = {"<:", ":>", "<%", "%>", "%:", "%:%:"}
EXPR_CHILDREN = {
    "unary": ("operand",),
    "binary": ("lhs", "rhs"),
    "assign": ("lhs", "rhs"),
    "conditional": ("cond", "then", "else_"),
    "call": ("callee", "args"),
    "subscript": ("base", "index"),
    "member": ("base",),
    "sizeof": ("operand",),
}

EXACT_CASES: List[Dict[str, object]] = [
    {
        "name": "expression-parenthesization",
        "source": "int main(void) { int a; int b; int c; "
                  "a = a * b + c; return a; }\n",
        "expected": "int (main(void))\n{\n\tint a;\n\tint b;\n\tint c;\n"
                    "\t(a = ((a * b) + c));\n\treturn a;\n}\n",
        "requirements": ["D-003", "D-005", "D-007"],
    },
    {
        "name": "declarator-parenthesization",
        "source": "int main(void) { int *p; int **q; int (*f)(int); "
                  "return 0; }\n",
        "expected": "int (main(void))\n{\n\tint (*p);\n\tint (*(*q));\n"
                    "\tint ((*f)(int));\n\treturn 0;\n}\n",
        "requirements": ["D-005"],
    },
    {
        "name": "no-reassociation",
        "source": "int main(void) { int a; int b; int c; "
                  "a = a + b + c; a = a ? b : c; a = -b; a = sizeof b; "
                  "a = sizeof(int); return a; }\n",
        "expected": "int (main(void))\n{\n\tint a;\n\tint b;\n\tint c;\n"
                    "\t(a = ((a + b) + c));\n\t(a = (a ? b : c));\n"
                    "\t(a = (-b));\n\t(a = (sizeof b));\n"
                    "\t(a = (sizeof(int)));\n\treturn a;\n}\n",
        "requirements": ["D-003", "D-005", "D-007"],
    },
    {
        "name": "digraph-canonicalization",
        "source": "int main(void) <% int *p; return p<:0:>; %>\n",
        "expected": "int (main(void))\n{\n\tint (*p);\n\treturn (p[0]);\n}\n",
        "requirements": ["D-006", "L-009"],
    },
]


def _parse(source: str):
    from c4ref.lexer import lex
    from c4ref.parser import parse
    return parse(lex(source))


def _expr_errors(expr, wrapped: bool, where: str) -> List[str]:
    if expr is None:
        return []
    if expr.kind == "paren":
        return _expr_errors(expr.operand, False, where)
    errors = []
    if wrapped and expr.kind not in ATOMS:
        errors.append("%s: %s expression is not parenthesized"
                      % (where, expr.kind))
    for field_name in EXPR_CHILDREN.get(expr.kind, ()):
        child = getattr(expr, field_name)
        if isinstance(child, list):
            for index, item in enumerate(child):
                errors += _expr_errors(item, True,
                                       "%s.%s[%d]" % (where, field_name,
                                                      index))
        else:
            errors += _expr_errors(child, True,
                                   "%s.%s" % (where, field_name))
    return errors


def _statement_errors(stmt, where: str) -> List[str]:
    errors = []
    if stmt is None:
        return errors
    if stmt.expr is not None:
        errors += _expr_errors(stmt.expr, True, where + ".expr")
    if stmt.cond is not None:
        errors += _expr_errors(stmt.cond, True, where + ".cond")
    for index, item in enumerate(stmt.items):
        errors += _statement_errors(item, "%s.items[%d]" % (where, index))
    errors += _statement_errors(stmt.then, where + ".then")
    errors += _statement_errors(stmt.else_, where + ".else")
    errors += _statement_errors(stmt.body, where + ".body")
    return errors


def _program_errors(program) -> List[str]:
    errors = []
    for index, decl in enumerate(program.decls):
        errors += _statement_errors(decl.body, "decl[%d].body" % index)
    return errors


def _bracket_balance(line: str) -> bool:
    depth = 0
    index = 0
    quote = None
    while index < len(line):
        ch = line[index]
        if quote:
            if ch == "\\":
                index += 2
                continue
            if ch == quote:
                quote = None
            index += 1
            continue
        if ch in ("'", '"'):
            quote = ch
            index += 1
            continue
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        index += 1
    return depth == 0


def _normalization_errors(output: str) -> List[str]:
    errors = []
    for number, line in enumerate(output.splitlines(), 1):
        stripped = line.lstrip(" \t")
        prefix = line[:len(line) - len(stripped)]
        if " " in prefix:
            errors.append("line %d uses spaces for indentation" % number)
        if prefix and prefix != "\t" * len(prefix):
            errors.append("line %d has mixed indentation" % number)
        if line != line.rstrip():
            errors.append("line %d has trailing whitespace" % number)
        if not _bracket_balance(line):
            errors.append("line %d wraps a parenthesized expression" % number)
    try:
        tokens = reference_tokens(output)
    except Exception as exc:
        return errors + ["printed output does not lex: %s" % exc]
    for token in tokens:
        if token[2] == "punctuator" and token[3] in DIGRAPHS:
            errors.append("digraph %r was printed" % token[3])
    return errors


def run_printer(ctx: Context, reporter: Reporter):
    # D-002: byte-exact reference file.
    fixture = ctx.root / "tests" / "fixtures" / "pretty.c"
    work = ctx.tempdir("printer-ref-")
    try:
        source = ctx.read_source(fixture)
        ctx.write_source(work / "pretty.c", source)
        proc = ctx.c4run("--print-ast", "pretty.c", work)
        expected = fixture.read_bytes().decode("latin-1")
        checks = Checks()
        checks.check(proc.returncode == 0,
                     "c4 exited %d: %s"
                     % (proc.returncode, proc.stderr[:200]))
        checks.check(proc.stdout == expected,
                     "output is not byte-identical to tests/fixtures/pretty.c")
        reporter.record("printer", "reference-file", ["D-001", "D-002"],
                        checks.ok, checks.detail())
    finally:
        ctx.cleanup(work)

    # D-001: rejection behavior matches --parse.
    work = ctx.tempdir("printer-reject-")
    try:
        ctx.write_source(work / "bad.c",
                         "int main(void) { return missing; }\n")
        proc = ctx.c4run("--print-ast", "bad.c", work)
        checks = Checks()
        checks.check(proc.returncode == 1,
                     "exit %d, expected 1" % proc.returncode)
        checks.check(not proc.stdout,
                     "rejected program produced stdout: %r" % proc.stdout[:100])
        checks.check("error:" in proc.stderr,
                     "missing diagnostic: %r" % proc.stderr[:160])
        reporter.record("printer", "rejection-behaviour", ["D-001"],
                        checks.ok, checks.detail())
    finally:
        ctx.cleanup(work)

    # Curated normalization outputs.
    for case in EXACT_CASES:
        name = str(case["name"])
        source = str(case["source"])
        expected = str(case["expected"])
        requirements = list(case["requirements"])
        work = ctx.tempdir("printer-exact-")
        try:
            ctx.write_source(work / "case.c", source)
            proc = ctx.c4run("--print-ast", "case.c", work)
            checks = Checks()
            checks.check(proc.returncode == 0,
                         "exit %d: %s"
                         % (proc.returncode, proc.stderr[:160]))
            checks.check(proc.stdout == expected,
                         "output mismatch:\n--- expected ---\n%s"
                         "--- actual ---\n%s" % (expected, proc.stdout))
            reporter.record("printer", name, requirements, checks.ok,
                            checks.detail())
        finally:
            ctx.cleanup(work)

    # Corpus round trips and structural checks.
    corpus = []
    programs_dir = ctx.root / "tests" / "e2e" / "programs"
    for path in sorted(programs_dir.glob("*.c")):
        corpus.append((path.stem, path.read_text(encoding="latin-1")))
    from .parser_cases import VALID_CASES
    for case in VALID_CASES:
        corpus.append(("valid-" + str(case["name"]), str(case["source"])))

    for name, source in corpus:
        work = ctx.tempdir("printer-")
        try:
            ctx.write_source(work / "case.c", source)
            first = ctx.c4run("--print-ast", "case.c", work)
            checks = Checks()
            checks.check(first.returncode == 0,
                         "print failed: %s" % first.stderr[:160])
            if first.returncode != 0:
                reporter.record("printer", name, ["D-001", "D-008"],
                                False, checks.detail())
                continue
            output = first.stdout
            ctx.write_source(work / "printed.c", output)
            reparse = ctx.c4run("--parse", "printed.c", work)
            checks.check(reparse.returncode == 0,
                         "printed program was rejected: %s"
                         % reparse.stderr[:160])
            second = ctx.c4run("--print-ast", "printed.c", work)
            checks.check(second.returncode == 0,
                         "second print failed: %s" % second.stderr[:160])
            checks.check(second.stdout == output,
                         "print-parse-print is not stable")
            try:
                original_ast = _parse(source)
                printed_ast = _parse(output)
            except Exception as exc:
                checks.check(False, "reference parse failed: %s" % exc)
                reporter.record("printer", name, ["D-003", "D-005", "D-007",
                                                  "D-008"], False,
                                checks.detail())
                continue
            checks.check(ast_equal(original_ast, printed_ast),
                         "printed structure differs from the source structure")
            checks.check(not _program_errors(printed_ast),
                         "; ".join(_program_errors(printed_ast)[:4]))
            errors = _normalization_errors(output)
            checks.check(not errors, "; ".join(errors[:4]))
            reporter.record("printer", name,
                            ["D-001", "D-003", "D-004", "D-005", "D-006",
                             "D-007", "D-008"], checks.ok, checks.detail())
        finally:
            ctx.cleanup(work)

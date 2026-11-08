"""Lexer conformance: token-for-token comparison with the reference lexer.

Each accepted case is compared exactly (kind, spelling, line, column).  Each
rejected case must be rejected by both the reference lexer and c4, with the
diagnostic location required by the specification unless the location is
recorded as ambiguous in ambiguities.md.
"""

from __future__ import annotations

from pathlib import Path
from typing import Dict, List

from .runner import (Context, Reporter, reference_tokens, parse_token_output,
                     DIAGNOSTIC_RE)

ALL_KEYWORDS = """auto break case char const continue default do double else
enum extern float for goto if inline int long register restrict return short
signed sizeof static struct switch typedef union unsigned void volatile while
_Alignas _Alignof _Atomic _Bool _Complex _Generic _Imaginary _Noreturn
_Static_assert _Thread_local"""

PUNCTUATORS = """[ ] ( ) { } . -> ++ -- & * + - ~ ! / % << >> < > <= >= == !=
^ | && || ? : ; ... = *= /= %= += -= <<= >>= &= ^= |= , # ##"""

ACCEPT_CASES: List[Dict[str, object]] = [
    {
        "name": "assignment-example",
        "source": '42  if\n "bla\\n"x+',
        "requirements": ["L-001", "L-004", "L-012"],
        "note": "AB.pdf --tokenize example",
    },
    {
        "name": "keywords",
        "source": ALL_KEYWORDS,
        "requirements": ["L-001", "L-003"],
    },
    {
        "name": "identifiers",
        "source": "_ a A z0 _0 ifx intx _Alignasx",
        "requirements": ["L-001", "L-004", "L-002"],
    },
    {
        "name": "decimal-constants",
        "source": "0 1 42 2147483647 999999999999999999999",
        "requirements": ["L-005"],
    },
    {
        "name": "character-constants",
        "source": r"'a' 'Z' '0' ' ' '\'' '\"' '\?' '\\' '\a' '\b' '\f' '\n' '\r' '\t' '\v'",
        "requirements": ["L-006", "L-008"],
    },
    {
        "name": "string-literals",
        "source": r'"" "abc" "a\nb" "\"" "\\" "a?b" "a:b"',
        "requirements": ["L-007", "L-008"],
    },
    {
        "name": "punctuators",
        "source": PUNCTUATORS,
        "requirements": ["L-009", "L-002"],
    },
    {
        "name": "digraph-spellings",
        "source": "<: :> <% %> %: %:%:",
        "requirements": ["L-009"],
    },
    {
        "name": "maximal-munch",
        "source": "a+++b a+++++b .... %:%: <<= -> && a---b",
        "requirements": ["L-002", "L-009"],
    },
    {
        "name": "whitespace-and-comments",
        "source": "a \t\v\f b /* x\ny */ c // tail\nd",
        "requirements": ["L-010"],
    },
    {
        "name": "positions-tabs-and-newlines",
        "source": "a\tb\r\nc\rd\ne\n",
        "requirements": ["L-011"],
    },
    {
        "name": "latin1-strings-and-comments",
        "source": 'char *s; s = "Gr\xfc\xdfe"; // \xe4\n',
        "requirements": ["A-008", "L-007"],
        "note": "ISO-8859-1 bytes inside a string and a comment",
    },
    {
        "name": "no-trigraph-replacement",
        "source": "??= ??/ ??( ??) ??! ??< ??> ??-",
        "requirements": ["A-010"],
    },
    {
        "name": "no-line-splicing",
        "source": "int a;\\\nint b;",
        "requirements": ["A-010"],
        "expect_reject": True,
        "ambiguous_location": True,
        "note": "phase 2 is omitted, so backslash-newline is an invalid character",
    },
    {
        "name": "no-adjacent-string-concatenation",
        "source": '"a" "b"',
        "requirements": ["A-010", "L-007"],
    },
    {
        "name": "hash-is-a-punctuator",
        "source": "# ## %: %:%: include",
        "requirements": ["A-009", "L-009"],
    },
    {
        "name": "no-preprocessor-header-name",
        "source": "#include <stdio.h>",
        "requirements": ["A-009"],
    },
    {
        "name": "comments-do-not-nest",
        "source": "/* a /* b */ c",
        "requirements": ["L-010"],
    },
    {
        "name": "empty-file",
        "source": "",
        "requirements": ["L-012", "L-015"],
    },
]

REJECT_CASES: List[Dict[str, object]] = [
    {
        "name": "invalid-character",
        "source": "int @;",
        "requirements": ["L-013", "A-004", "A-007"],
    },
    {
        "name": "leading-zero-octal",
        "source": "01",
        "requirements": ["L-005", "L-013"],
    },
    {
        "name": "leading-zero-nine",
        "source": "09",
        "requirements": ["L-005", "L-013"],
    },
    {
        "name": "hexadecimal-constant",
        "source": "0x10",
        "requirements": ["L-005", "L-013"],
    },
    {
        "name": "integer-suffix",
        "source": "123u",
        "requirements": ["L-005", "L-013"],
    },
    {
        "name": "floating-constant",
        "source": "1.5",
        "requirements": ["L-005", "L-013"],
    },
    {
        "name": "empty-character-constant",
        "source": "''",
        "requirements": ["L-006", "L-013"],
    },
    {
        "name": "multi-character-constant",
        "source": "'ab'",
        "requirements": ["L-006", "L-013"],
        "ambiguous_location": True,
    },
    {
        "name": "octal-escape-in-character",
        "source": r"'\0'",
        "requirements": ["L-008", "L-013"],
    },
    {
        "name": "hex-escape-in-character",
        "source": r"'\x41'",
        "requirements": ["L-008", "L-013"],
    },
    {
        "name": "ucn-escape-in-string",
        "source": r'"\u0041"',
        "requirements": ["L-008", "L-013"],
    },
    {
        "name": "unterminated-string",
        "source": '"abc',
        "requirements": ["L-007", "L-013"],
        "ambiguous_location": True,
    },
    {
        "name": "newline-in-string",
        "source": '"a\nb"',
        "requirements": ["L-007", "L-013"],
        "ambiguous_location": True,
    },
    {
        "name": "unterminated-block-comment",
        "source": "int x; /* comment",
        "requirements": ["L-010", "L-013"],
        "ambiguous_location": True,
    },
    {
        "name": "latin1-identifier",
        "source": "int \xe4;",
        "requirements": ["L-004", "L-013", "A-011"],
        "may_reject": True,
    },
]


def _token_summary(tokens):
    return " ".join("%d:%d:%s:%s" % token for token in tokens[:12])


def run_lexer(ctx: Context, reporter: Reporter):
    for case in ACCEPT_CASES + REJECT_CASES:
        name = str(case["name"])
        text = str(case["source"])
        requirements = list(case["requirements"])
        expect_reject = bool(case.get("expect_reject")) or case in REJECT_CASES
        work = ctx.tempdir("lex-")
        try:
            source_name = "case.c"
            ctx.write_source(work / source_name, text)
            proc = ctx.c4run("--tokenize", source_name, work)

            if expect_reject:
                reference_error = None
                try:
                    reference_tokens(text)
                except Exception as exc:  # LexError
                    reference_error = exc
                if reference_error is None:
                    reporter.check("lexer", name, requirements, False,
                                   "reference lexer accepted a reject case")
                    continue
                if proc.returncode != 1:
                    reporter.check(
                        "lexer", name, requirements, False,
                        "c4 exit %d, expected 1 (stderr %r)"
                        % (proc.returncode, proc.stderr[:200]))
                    continue
                matches = [DIAGNOSTIC_RE.match(line)
                           for line in proc.stderr.splitlines()]
                matches = [m for m in matches if m]
                if not matches:
                    reporter.check(
                        "lexer", name, requirements, False,
                        "missing diagnostic: %r" % proc.stderr[:200])
                    continue
                match = matches[0]
                same_location = (int(match.group("line")) ==
                                 reference_error.line and
                                 int(match.group("column")) ==
                                 reference_error.column)
                if case.get("ambiguous_location"):
                    reporter.record(
                        "lexer", name, requirements, True,
                        "rejected at %s:%s (reference %d:%d; location "
                        "ambiguous)" % (match.group("line"),
                                        match.group("column"),
                                        reference_error.line,
                                        reference_error.column))
                else:
                    reporter.check(
                        "lexer", name, requirements, same_location,
                        "diagnostic at %s:%s, reference %d:%d (%s)"
                        % (match.group("line"), match.group("column"),
                           reference_error.line, reference_error.column,
                           reference_error.message))
                continue

            # Accepted token stream.
            try:
                expected = reference_tokens(text)
            except Exception as exc:
                reporter.check("lexer", name, requirements, False,
                               "reference lexer rejected accept case: %s" % exc)
                continue
            if proc.returncode != 0:
                reporter.check(
                    "lexer", name, requirements, False,
                    "c4 exit %d (stderr %r)"
                    % (proc.returncode, proc.stderr[:200]))
                continue
            actual, malformed = parse_token_output(proc, source_name)
            checks = []
            if malformed:
                checks.append("malformed token lines: %r" % malformed[:3])
            if actual != expected:
                first = next((i for i, (a, e) in enumerate(zip(actual, expected))
                              if a != e), min(len(actual), len(expected)))
                checks.append(
                    "token mismatch at %d: c4=%r reference=%r"
                    % (first,
                       actual[first] if first < len(actual) else None,
                       expected[first] if first < len(expected) else None))
            if len(actual) != len(expected):
                checks.append("token count %d, reference %d"
                              % (len(actual), len(expected)))
            if "end-of-input" in proc.stdout:
                checks.append("EOF token was printed")
            reporter.check(
                "lexer", name, requirements, not checks,
                "; ".join(checks) if checks else
                "%d tokens match" % len(actual))
        finally:
            ctx.cleanup(work)

    _check_linear_time(ctx, reporter)


def _check_linear_time(ctx: Context, reporter: Reporter):
    """Input-size scaling evidence for L-014 (not a proof of O(n))."""
    work = ctx.tempdir("lex-time-")
    try:
        timings = []
        for count in (100000, 200000, 400000):
            source_name = "scale_%d.c" % count
            text = " ".join(["x"] * count)
            ctx.write_source(work / source_name, text)
            best = None
            for _ in range(2):
                proc = ctx.c4run("--tokenize", source_name, work,
                                 timeout=120.0)
                if proc.returncode != 0:
                    reporter.check("lexer", "linear-time", ["L-014"], False,
                                   "tokenize failed at size %d" % count)
                    return
                best = proc.seconds if best is None else min(best, proc.seconds)
            timings.append((count, best))
        small, medium, large = timings
        ratio = large[1] / max(small[1], 1e-6)
        # Four times the input should not behave quadratically (ratio ~16).
        reporter.check(
            "lexer", "linear-time", ["L-014"], ratio < 12.0,
            "100k/200k/400k tokens took %.3fs/%.3fs/%.3fs (4x ratio %.2f)"
            % (small[1], medium[1], large[1], ratio))
    finally:
        ctx.cleanup(work)

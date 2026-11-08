# C4 observable contracts (Assignments 1–5)

Normative sources: \`AB.pdf\` (Task A), \`CD.pdf\` (Tasks C–D), \`E.pdf\`
(Task E), \`F.pdf\` (Task F), \`G.pdf\` (Task G).  Requirement identifiers are
referenced by \`verification/spec/requirements.yaml\`.

## A-001 — Acceptance boundary

AB.pdf, Project Task A:

- A program that is in the specified C4 subset and satisfies the C11
  constraints must be accepted and correctly compiled.
- A program that is not part of the full C language must be rejected.
- A program that is valid C but outside C4 may either be rejected with an
  error message or implemented correctly according to the C language
  specification.  It must not be silently miscompiled.
- Explicit Task E exemptions (S-019) may be rejected even though they are
  valid C; they are not required acceptance cases.

The conformance suite therefore classifies every generated input as one of:

1. **required-accept**: in the restricted grammar and accepted by the
   reference Sema checker (or an explicitly supported extension);
2. **must-reject**: outside the C language (lexical, syntactic, or semantic
   invalidity), including invalid C4 programs;
3. **may-reject**: valid C outside C4 (e.g. \`for\`, \`typedef\`, division,
   arrays, old-style declarators);
4. **exempted**: valid C that Task E explicitly does not require (struct
   assignment, struct parameters/returns, anonymous members, \`__func__\`).

## A-002 — Build and executable

AB.pdf: the project must be buildable with the provided Makefile, producing a
binary \`c4\`.

Verification: build the repository with its declared build command and run
the produced binary.  The C++-language requirement is an implementation
constraint, not a property established by the conformance suite.

## A-003 — Exit codes and diagnostics

AB.pdf:

- Acceptance terminates with return code 0.
- Rejection terminates with return code 1 and prints a diagnostic to stderr:
  \`<filename>:<line>:<column>: error: <message>\`.
- \`<filename>\` is the input file name as provided on the command line.
- \`<line>\` and \`<column>\` are 1-based source positions.
- The message is meaningful but its exact wording is not specified.

The conformance suite checks exit codes and the diagnostic prefix/location
shape.  It does not require exact message text except where a test explicitly
records the implementation's message for regression.

## A-004 — CLI modes

AB.pdf, CD.pdf, F.pdf, G.pdf:

| Invocation | Behavior |
|---|---|
| \`c4 FILE\` | same as \`--compile FILE\` |
| \`c4 --tokenize FILE\` | print the token stream (L-012) |
| \`c4 --parse FILE\` | parse and semantically analyze; no output on success |
| \`c4 --print-ast FILE\` | parse, analyze, pretty-print to stdout (D-001) |
| \`c4 --compile FILE\` | emit LLVM IR to \`FILE_BASENAME.ll\` in the current directory (F-001) |
| \`c4 --optimize FILE\` | same as \`--compile\` plus SCCP (G-001) |

\`--help\`/\`-h\` are implementation conveniences, not assignment requirements.
An unknown mode or wrong argument count is an invocation error, not a source
program rejection; the assignment does not prescribe its exit code.  The
conformance suite does not treat invocation errors as source acceptance.

## D-001 — Pretty-printer mode

CD.pdf, Project Task D:

- \`c4 --print-ast FILE\` prints the pretty-printed program to stdout.
- It augments \`--parse\`: acceptance/rejection behavior and return codes are
  the same as \`--parse\`.
- For a syntactically and semantically correct input, the output is source
  code; no diagnostic is printed to stdout.

## D-002 — Reference output

CD.pdf: invoking \`c4 --print-ast ex05_pretty.c\` must emit a char-exact copy
of the provided reference file.  The repository's
\`tests/fixtures/pretty.c\` is the reference file used by the existing tests.
The conformance suite compares the output byte-for-byte with that file and
also checks that the output itself re-parses.

## D-003 — Normalization rules

CD.pdf, Project Task D:

- Print everything in the original order of the source file.
- Use one tab character (\`\\t\`) per indentation level; do not use spaces for
  indentation.
- Do not wrap long lines.
- Do not print digraphs; print the regular punctuator instead.
- Each (sub-)expression and each (sub-)declarator/abstract-declarator must be
  fully parenthesized, except for atoms consisting of at most one token
  (\`0\`, \`'c'\`, \`"abc"\`, identifiers, etc.).  Original parentheses are
  discarded.  Do not re-associate subexpressions.
- Example: \`a * b + d + e\` is printed as \`(((a * b) + d) + e)\`.
- Newlines, spacing, and brace placement follow the reference file (mostly
  K&R style).

The reference pretty-printer implements these rules from the AST.  Structural
round trips check that printing is stable: \`print(parse(print(parse(src)))) =
print(parse(src))\`, and the reprinted output is accepted by the parser.

## F-001 — LLVM IR output

F.pdf, Project Task F:

- The compiler performs systematic LLVM IR construction when \`--compile\` is
  given or no explicit compilation mode is given.
- \`char\` is represented by LLVM's 8-bit integer type and \`int\` by the
  32-bit integer type.
- LLVM's \`DataLayout\` may be used to compute sizes of types.
- The output is human-readable LLVM code in a file.  For input
  \`foo/bar.c\`, the output file is \`bar.ll\` in the current directory.
- The assignment names LLVM 15.0 as the project environment.  This repository
  is built against the installed LLVM 23.1.5; that is a toolchain deviation
  recorded in ambiguities.md.  The IR-level requirements above are
  independent of the version, but compatibility with LLVM 15 is not claimed.
- The emitted module must pass LLVM's verifier and be parseable by
  \`llvm-as\`.  This is a necessary well-formedness check, not a proof of
  semantic correctness.

## F-002 — Code generation and defined behavior

F.pdf requires IR construction, not a source-to-IR refinement proof.  The
conformance suite checks:

- the module is valid LLVM IR;
- for defined C4 programs, the c4-compiled executable and a C reference
  executable (compiled from the same source with the system C compiler)
  produce the same observable result (exit status and stdout);
- the default mode and \`--compile\` produce the same IR for the same input;
- the output naming rule \`foo/bar.c -> bar.ll\` holds.

Differential execution is evidence on the tested programs, not a proof for
all programs.  Programs are restricted to defined behavior and avoid
implementation-defined differences (for example, \`char\` signedness and
struct padding) unless the test accounts for them explicitly.

## G-001 — SCCP mode

G.pdf, Project Task G:

- \`--optimize\` performs the same operation as \`--compile\` and additionally
  performs optimizations.
- The optimization is SCCP (Wegman–Zadeck).
- If applicable, the pass must:
  1. substitute instructions with constants;
  2. substitute conditional branches with unconditional ones;
  3. remove dead instructions;
  4. remove unreachable blocks.
- The implementation may use LLVM's \`mem2reg\` pass but no other LLVM
  passes.

## G-002 — SCCP verification obligations

The assignment states the transformations but not a formal refinement
calculus.  The conformance suite verifies each transformation on targeted IR
fixtures derived from C4 source:

- **constant substitution**: a constant-valued instruction is replaced by a
  constant operand or a constant result in the post-SCCP IR;
- **branch simplification**: a conditional branch whose condition is a known
  constant becomes an unconditional branch (or is removed with its
  unreachable successor);
- **dead-instruction elimination**: instructions proven dead by the
  transformation are absent from the post-SCCP IR;
- **unreachable-block elimination**: blocks proven unreachable are absent.

Each fixture captures the pre-SCCP and post-SCCP IR for comparison, records
the expected transformation, and whether it was observed.  The post-SCCP
module must pass the LLVM verifier.  Behavior regressions compile and run the optimized
program and compare its observable result with the unoptimized program on
defined inputs.  A transformation that is not applicable to a fixture is
reported as not-applicable, not as verified.

## G-003 — Pre/post IR capture

The C4 implementation runs \`mem2reg\` immediately before its custom SCCP.
The conformance harness captures:

- \`c4 --compile\`: the pre-mem2reg IR;
- the same IR after the LLVM \`mem2reg\` pass: the pre-SCCP IR;
- \`c4 --optimize\`: the post-SCCP IR.

The harness checks that the external \`mem2reg\` result is the exact pre-SCCP
input by textual comparison where possible; if the two mem2reg runs differ,
the fixture records that limitation and the comparison is made against the
actual post-SCCP output only for the transformations that can be identified
structurally.  The harness does not substitute LLVM's SCCP.

## A-005 — Error location requirements

- Lexer: first character that makes the input invalid (L-013).
- Parser: the start of the first token that makes the token sequence not a
  word of the grammar; if a token is missing at the end of the token
  sequence, the end-of-file position (CD.pdf, Task C).
- Sema: the first terminal of the syntactic construct where the error was
  detected (E.pdf, Task E; S-018).

The conformance suite checks locations for the categories listed in L-013,
C-009, and S-018.  Ambiguous categories are recorded in ambiguities.md and
are not counted as confirmed defects.

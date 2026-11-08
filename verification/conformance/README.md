# C4 conformance harness

This directory contains the executable Stage 2 conformance suite for the
Assignment A–G requirements.  It drives the real compiler (`build/c4`) and
compares it with the independent reference frontend in `c4ref/`.  The
normative sources and requirement identifiers are in `../spec/`; the
generated coverage matrix is `../COVERAGE.md`.

## Requirements

- Python 3.9+ with PyYAML
- a built `c4` binary (`make` or `cmake --build build --target c4`)
- LLVM tools `llvm-as` and `opt` (found on `PATH` or under
  `/opt/homebrew/opt/llvm/bin`)
- a C compiler (`clang`) for the differential execution checks

## Run

```sh
python3 verification/conformance/run_conformance.py --c4 build/c4
```

The command writes `results/evidence.json` and regenerates `../COVERAGE.md`.
To also write the executed statuses and evidence summaries back into
`verification/spec/requirements.yaml`:

```sh
python3 verification/conformance/run_conformance.py --c4 build/c4 \
  --update-requirements
```

Use `--suite lexer,parser,sema` for a subset, `--verbose` for per-case
output, and `--keep` to keep temporary files.  The process exits non-zero if
any case fails.

## Suites

| suite | checks | main requirements |
|---|---|---|
| build | binary, Makefile targets, C++ sources | A-002, A-006 |
| cli | modes, exit codes, diagnostics, output naming | A-003–A-005, A-007, F-004, F-006 |
| lexer | exact token streams and lexical errors against `c4ref.lexer` | L-001–L-015, A-008–A-010 |
| parser | curated grammar cases, positive coverage of each grammar alternative, and token-deletion mutants; may-reject classified with a C-validity oracle | C-001–C-011, A-011, A-012 |
| sema | accept/reject/exempt agreement with `c4ref.sema` and selected error locations | S-001–S-030 |
| printer | byte-exact reference file, normalization, full parenthesization, structural round trips | D-001–D-008 |
| codegen | `llvm-as`/verifier, i8/i32, DataLayout, differential execution in both modes | F-001–F-008 |
| sccp | pre/post IR pairs, the four transformations, verifier, behavior preservation, pass inventory | G-001–G-010 |

## Classification and status policy

Every input is one of: required C4 behaviour, invalid C that must be
rejected, valid C outside C4 that may be rejected (and is accepted only as a
recorded extension), or an explicit Task E exemption.  A requirement whose
mapped cases all pass is corpus-verified: the finite corpus conforms, but
this is not a proof for untested inputs.  verified is reserved for a
machine-checked universal claim; no requirement currently has one.  A
requirement with a failing or skipped case is partial, one whose check
cannot execute is blocked, and one with no mapped case stays uncovered.
Out-of-scope items are not-applicable.

The parser suite also tracks every alternative of the grammar in
../spec/grammar.ebnf.  Each distinct positive example is a parser/grammar-*
case; alternatives not exercised by an accepted example are listed
explicitly in the report.

## Evidence provenance

The evidence JSON records the exact binary path and SHA-256, its mtime, the
newest source mtime and a stale flag, the source revision, and the
verification and conformance tree revisions.  A binary older than the
sources is reported as stale rather than being presented as freshly built.
The command line, working directory, Python version, and working-tree
changes are also recorded.  Reproduce a run with the command in the evidence
file after building the recorded source revision.

## Adding cases

Add cases to the relevant `suite/*_cases.py` list with a stable name and the
requirement ids they exercise.  Accepted parser/sema cases must be accepted
by the reference frontend; rejected cases must be rejected by it.  After a
change, run the full suite and regenerate the coverage report.

## Out of scope

Alive2, general SCCP refinement proofs, LLVM operational semantics, and C++
implementation-refinement proofs are deliberately not part of this harness.

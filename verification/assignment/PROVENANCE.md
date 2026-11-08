# Assignment sources and provenance

This directory contains the authoritative project assignment documents for
Compiler Construction (WS 2022/23), Saarland University, and the C11
Committee Draft N1570 that the assignments reference.

## Authoritative sources

| File | Assignment | Pages | SHA-256 |
|---|---|---|---|
| `AB.pdf` | Project Assignment 1 — Task A (general information) and Task B (lexer) | 2 | `a64e93cfcdf2e6151494c5b46e45d45e814f2f0fc9db65e13b082352e03b14e7` |
| `CD.pdf` | Project Assignment 2 — Task C (parser) and Task D (pretty printing) | 2 | `01e9510c93e0f842e3355eba37f9e6ea53dc1cdf174e58c1710666bfd025b7d7` |
| `E.pdf` | Project Assignment 3 — Task E (semantic analysis) | 1 | `334df4ee69474de784189781c450d07300d216f7cc157c1946dcda3508b8e6e9` |
| `F.pdf` | Project Assignment 4 — Task F (LLVM IR) | 1 | `12fb6176c9d6ce928c6375b0f2775439f80ae1f41b5e5d2845c518f90d64b367` |
| `G.pdf` | Project Assignment 5 — Task G (SCCP) | 1 | `e39aee8da4c0425e603992b70cbdc595cad2954cd794ba9351072a595294a486` |
| `n1570.pdf` | ISO/IEC 9899:201x Committee Draft N1570 (C11) | 701 | `208969fec021e4f4ce87a1873765298c73e7bbb5209b1c5868967c7adfc8492e` |

The PDFs are the normative sources. The files in `../extracted/` are
UTF-8 conveniences produced with `pdftotext -layout` from the PDFs and are
not independent evidence. All normative statements in `../spec/` cite the
PDF and the section/clause; where a statement is an interpretation, it is
recorded in `../spec/ambiguities.md`.

The C++ implementation, its tests, `README.md`, and `DESIGN.md` are not
normative sources. They are consulted only to identify the implementation
interface named in the traceability matrix.

## Reproduction of the extracted text

```sh
cd verification/assignment/sources
for f in AB CD E F G; do
  pdftotext -layout "$f.pdf" "../extracted/$f.txt"
  iconv -f ISO-8859-1 -t UTF-8 "../extracted/$f.txt" > "../extracted/$f.utf8.txt"
done
```

The assignment PDFs are encoded with the ISO-8859-1 character set. The
extracted `.utf8.txt` files are converted from that encoding; the `.txt`
intermediate files are not retained.

## Scope of the assignment

- Assignment 1 (AB) states the acceptance boundary, the build and error
  contract, and the lexical rules.
- Assignment 2 (CD) restricts the C11 grammar to C4 and specifies the
  pretty printer.
- Assignment 3 (E) adds the applicable C11 semantic constraints and lists
  explicit exemptions.
- Assignment 4 (F) requires systematic LLVM IR construction.
- Assignment 5 (G) requires SCCP and the four transformations named there.

C11 N1570 is incorporated only where an assignment clause refers to it
(notably §6.4, §6.5, §6.7, §6.8, §6.9, and the Constraints/Semantics
clauses of Task E). General C11 requirements that the assignments do not
incorporate are not promoted to C4 obligations.

# Interpretations of ambiguous assignment wording

This file records every place where the assignment wording does not determine
a unique formal rule.  Each entry states the interpretation used by the
specification and the textual evidence.  Interpretations are not silently
promoted to normative requirements; the traceability matrix marks the
affected requirements.

## I-001 — "In phase 1, trigraphs are elided"

**Interpretation:** trigraph replacement is not performed.  A trigraph such as
\`??=\` is not translated to \`#\`; its characters are tokenized normally
(\`?\`, \`?\`, \`=\`).

**Evidence:** AB.pdf says "We leave out several parts. In phase 1, trigraphs
are elided."  The surrounding sentence lists omitted phases; "elided" is read
as "omitted from the required processing", not "removed from the input".
C4's observed behavior is consistent: \`??=\` yields three punctuators.

## I-002 — Tab width in source positions

**Interpretation:** a tab advances the column by one, like any other
non-newline character.  The assignment defines columns but does not specify a
tab stop.

**Evidence:** AB.pdf defines positions as line/column pairs but says nothing
about tab stops.  C4's observed \`int\tx;\` reports \`x\` at column 5, i.e. one
column for the tab.

## I-003 — Location of an unterminated block comment

**Interpretation:** report the first character of the comment (the \`/\`).

**Evidence:** AB.pdf requires "the location of the first character that makes
the input invalid".  The comment becomes invalid only when EOF is reached
without \`*/\`, so the literal first-invalid character could be EOF; the
opening \`/\` is the first character of the invalid construct.  C4 reports the
opening \`/\`; the conformance suite treats this as the implementation's
interpretation and does not count a different defensible location as a
defect.

## I-004 — Location of an unterminated string or character constant

**Interpretation:** report the opening quote.

**Evidence:** same reasoning as I-003.  C4 reports the opening quote for
\`"a<newline>b"\`.  The assignment's "first character that makes the input
invalid" is ambiguous between the opening quote and the newline.

## I-005 — Integer suffixes and prefixes

**Interpretation:** \`1u\`, \`1L\`, \`0x10\`, and \`1.0\` are lexical errors at the
first character after the admissible digit sequence, not a valid constant
followed by an identifier or punctuator.

**Evidence:** AB.pdf: "we process only decimal-constants, 0 and
character-constants. In this connection we ignore integer-suffixes and
prefixes for the encoding".  C11's \`integer-constant\` production would make
\`1u\` one token; ignoring the suffix means that token is not admitted.  C4
reports "invalid suffix on integer constant" at the suffix character.

## I-006 — Extended identifier characters

**Interpretation:** only ASCII letters, digits, and underscore form
identifiers.  ISO-8859-1 bytes outside ASCII are invalid in identifiers but
are valid source characters inside character constants and string literals.

**Evidence:** AB.pdf says the input encoding is ISO-8859-1 and that
universal-character-names are omitted.  C11 §6.4.2.1 permits implementation-
defined extended identifier characters; C4 rejects them.  Accepting them is
valid-C-outside-C4 and may be rejected.  Strings containing Latin-1 bytes are
observed to be accepted.

## I-007 — Empty parameter list \`f()\`

**Interpretation:** \`f()\` is outside the C4 grammar.  Task C admits only a
function declarator with \`parameter-type-list\`, and the C11
\`parameter-type-list\` has at least one parameter-declaration.  A no-parameter
function must be written \`f(void)\`.

**Evidence:** CD.pdf, Task C: "function declarator with parameter-type-list.
parameter-type-list is only parameter-list without ellipses".  C11
§6.7.6.3p14's empty identifier list is therefore excluded.  C4 accepts
\`int f();\` as an extension; the conformance suite classifies it as
valid-C-outside-C4 (may accept or reject).

## I-008 — Array declarators

**Interpretation:** array declarators are outside C4 even though the
subscript expression \`[]\` is in the expression subset.  Arrays are still
reachable through string literals (\`char[N]\`).

**Evidence:** CD.pdf's declarator list names pointer, identifier,
parenthesized declarator, and function declarator; it does not name an array
declarator.  The expression list separately includes \`[]\`.

## I-009 — Anonymous structs vs untagged struct declarations

**Interpretation:** an "anonymous struct" in the Task E exemption is the C11
§6.7.2.1p13 unnamed member.  Untagged struct declarations with named members
(\`struct { int x; } s;\`) remain in C4.

**Evidence:** E.pdf: "It is not necessary to accept programs that contain
anonymous structs (in the sense this term is used in the standard; struct
declarations without tags are still part of our language subset)."

## I-010 — Bare untagged struct declaration

**Interpretation:** \`struct { int x; };\` (no declarator, no tag) is
syntactically derivable from the restricted grammar but violates C11
§6.7p2 because it declares no declarator and no tag.  The reference checker
rejects it.  C4's behavior is measured; a difference is classified as an
ambiguity unless it also accepts an invalid-C construct that the assignment
requires to be rejected.

**Evidence:** C11 §6.7p2; CD.pdf allows \`init-declarator-list\` with at most
one init-declarator but does not explicitly require at least one.

## I-011 — Declaration with no declarator

**Interpretation:** \`int;\` is invalid (C11 §6.7p2); \`struct S;\` is valid
because it declares a tag.

**Evidence:** C11 §6.7p2.

## I-012 — Null pointer constant

**Interpretation:** only the integer-constant token \`0\` is a null pointer
constant.  \`(0)\`, \`0 + 0\`, and \`'\0'\` are not.

**Evidence:** E.pdf: "The only null pointer constant that you need to support
is literal 0."  C11 §6.3.2.3p3 defines a null pointer constant more broadly
(integer constant expression with value 0, or such an expression cast to
\`void *\`), but Task E narrows the required support to the literal.

## I-013 — Struct assignment, parameters, and returns

**Interpretation:** these are exempted, not required.  A rejection is
conforming.  If the implementation accepts them, they are outside the
required conformance domain and are tested only as optional behavior.

**Evidence:** E.pdf: "It is not necessary to accept programs that contain
functions which return a struct or have one as parameter. Similarly, it is
not necessary to accept programs that contain assignments of struct type."

## I-014 — Incomplete tentative definition at end of translation unit

**Interpretation:** a file-scope object that remains a tentative definition
of incomplete type at the end of the translation unit is rejected.

**Evidence:** C11 §6.9.2p2 defines the object as if it had an initializer of
0; an object definition requires a complete type.  C11 §6.9.2p3 states the
rule explicitly for internal linkage; C4 has only external linkage, so the
interpretation is recorded here.  C4 rejects such programs (existing
regression \`tests/e2e/rejects/tentative_incomplete.c\`).

## I-015 — \`sizeof\` result type and value

**Interpretation:** the result type is \`int\` (Task E's \`size_t = int\`).
The numeric value is implementation-defined and must agree with the LLVM
DataLayout used for code generation; Sema needs the type, not the value, for
acceptance.

**Evidence:** E.pdf: "Use int for the types ptrdiff_t and size_t."  F.pdf
allows \`llvm::DataLayout\` for sizes.

## I-016 — \`char\` signedness

**Interpretation:** the signedness of \`char\` is implementation-defined and
is not part of the acceptance contract.  Differential execution tests that
depend on \`char\` signedness either avoid the dependency or record the
implementation's choice.

**Evidence:** C11 §6.2.5p15; the assignment does not specify signedness.

## I-017 — Function-call error location

**Interpretation:** a call arity/type error is reported at the first token of
the callee expression (the start of the syntactic construct).  C4 may report
the call's \`(\` or the offending argument; the conformance suite compares
locations only where the assignment's example is unambiguous, and records
call-location differences as ambiguous.

**Evidence:** E.pdf: "use the location of the (first) terminal of the
syntactic construct where the error was detected."

## I-018 — Declaration/type error location

**Interpretation:** declaration-level errors are reported at the declaration's
first token; name-resolution errors at the offending identifier.  C4 may use
the declarator or type specifier; location differences in this category are
recorded as ambiguous unless the assignment gives a specific example.

**Evidence:** E.pdf's examples cover operators and \`if\`, not declarations.

## I-019 — Duplicate parameter names in prototypes

**Interpretation:** the assignment does not state a rule.  The reference
checker does not reject duplicate parameter names in a prototype solely for
duplication; it checks each parameter's type.  C4's behavior is measured and
recorded, not treated as a required rule.

**Evidence:** C11 does not contain an explicit constraint forbidding
duplicate parameter names in a prototype; the names have prototype scope.

## I-020 — LLVM version

**Interpretation:** the repository is built against LLVM 23.1.5, not the
LLVM 15.0 named by F.pdf.  The IR construction requirements (i8/i32,
DataLayout, human-readable output, file naming) are checked against the
installed toolchain.  Compatibility with LLVM 15 is not claimed.

**Evidence:** F.pdf says "Install LLVM 15.0"; the existing project build and
the governing task use LLVM 23.1.5.  This is a toolchain deviation, not a
language-subset deviation.

## I-021 — Adjacent string literals

**Interpretation:** adjacent string literals are not concatenated because
translation phase 6 is omitted.  \`"a" "b"\` is two tokens and is not a valid
primary expression.

**Evidence:** AB.pdf: "Phases 2, 4 and 6 are left out."

## I-022 — Digraphs

**Interpretation:** digraphs are lexed as distinct punctuator tokens and
preserved in \`--tokenize\` output; the parser and printer canonicalize them
to \`[ ] { } # ##\`.

**Evidence:** AB.pdf: "do not replace digraphs with their more common
counterparts" in token output; CD.pdf: "Do not print digraphs. Print the
regular punctuator instead."

## I-023 — Labels before declarations

**Interpretation:** \`label: int x;\` is outside the restricted grammar
because a declaration is not a \`statement\` and \`labeled-statement\` requires
a \`statement\`.

**Evidence:** CD.pdf's statement list; C11 §6.8.1's grammar.

## I-024 — Reaching the end of a non-void function

**Interpretation:** not a constraint violation; the reference checker does
not reject a non-void function merely because control can reach its closing
brace.  C11 §6.9.1p12 makes the behavior undefined only if the caller uses
the value.

**Evidence:** C11 §6.9.1p12.

## I-025 — Evaluation order and undefined behavior

**Interpretation:** the Sema conformance suite checks name/type rules, not
evaluation order, sequencing, or undefined-behavior freedom.  CodeGen
differential tests use programs whose behavior is defined.

**Evidence:** Task E limits semantic analysis to name/type analysis and the
Constraints/Semantics clauses; it explicitly says expression values need not
be evaluated.

## I-026 — SCCP pre-IR capture

**Interpretation:** the external \`mem2reg\` result is used as the pre-SCCP IR
when it is textually identical to the implementation's mem2reg result.  If
they differ, the fixture records the difference and verifies only the
transformations that are structurally identifiable in the actual post-SCCP
IR.

**Evidence:** C4 runs \`runMem2Reg\` immediately before \`SCCP\`; the
assignment permits \`mem2reg\` and no other LLVM passes.  This is a harness
limitation, not an assignment requirement.

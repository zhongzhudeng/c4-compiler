# C4 lexical specification (Assignment 1, Project Task B)

Normative source: \`verification/assignment/sources/AB.pdf\`, Project Task B,
page 2, and the C11 clauses it cites.  C11 N1570 §6.4 is incorporated only to
the extent stated below.

Requirement identifiers in this file are referenced by
\`verification/spec/requirements.yaml\`.

## L-001 — Input encoding and translation phases

- The input is a byte stream interpreted as ISO-8859-1 (AB.pdf, Task B,
  paragraph 2: "For the input and execution encodings we use ISO-8859-1").
- Only the lexical phase is specified here.  Phase 1 trigraph replacement is
  **not** performed (AB.pdf: "In phase 1, trigraphs are elided").  Phases 2
  (line splicing), 4 (preprocessing directives), and 6 (adjacent string
  literal concatenation) are omitted (AB.pdf: "Phases 2, 4 and 6 are left
  out").  Phase 3 (tokenization and comment removal) is performed.
- There is no preprocessor.  The lexer produces C \`token\`s, not
  \`preprocessing-token\`s (AB.pdf: "we are only considering tokens, not
  preprocessing-tokens").  Consequently header names are not tokens and
  \`#\`/\`##\` are ordinary punctuators.
- Universal-character-names are omitted (AB.pdf).  A backslash outside a
  character constant or string literal is an invalid character.

## L-002 — Source positions

- A position is a pair \`(line, column)\`, both counting from 1 (AB.pdf,
  Task B, \`--tokenize\` paragraph).
- A newline is a CR LF sequence, or a lone CR or LF that is not part of a
  CR LF sequence (AB.pdf).  CR LF advances the line once, not twice.
- A newline resets the column to 1.  Every other source character advances
  the column by one.  In particular a tab counts as one column (this is an
  interpretation; see ambiguities.md).
- The position of a token is the position of its first character.
- The end-of-file position is the position after the last character of the
  file.  If the file ends in a newline, it is line \`n+1\`, column 1.

## L-003 — Whitespace and comments

- Whitespace is space (U+0020), horizontal tab (U+0009), vertical tab
  (U+000B), form feed (U+000C), carriage return (U+000D), and line feed
  (U+000A), interpreted per L-002.
- A \`/* ... */\` comment is whitespace and may span lines.  A \`//\` comment
  extends to, but not including, the next newline.  Comments produce no
  tokens.
- An unterminated \`/*\` comment is a lexical error.  Its reported location is
  the first character of the comment (interpretation; see ambiguities.md).
- Comments do not nest; the first \`*/\` terminates a block comment.

## L-004 — Token classes

The token classes are those of the \`token\` nonterminal of C11 §6.4p1 that
remain after preprocessing tokens are excluded:

\`\`\`
token ::= keyword | identifier | constant | string-literal | punctuator
\`\`\`

The \`--tokenize\` output uses exactly these five kind names.

## L-005 — Keywords

A keyword is one of the C11 §6.4.1 keywords:

\`\`\`
auto break case char const continue default do double else enum extern
float for goto if inline int long register restrict return short signed
sizeof static struct switch typedef union unsigned void volatile while
_Alignas _Alignof _Atomic _Bool _Complex _Generic _Imaginary _Noreturn
_Static_assert _Thread_local
\`\`\`

Keywords are recognized by maximal munch on identifier-like characters and
are emitted with kind \`keyword\`.  The parser accepts only the C4 subset;
recognizing a keyword does not imply that the parser accepts it.

## L-006 — Identifiers

Without universal-character-names, an identifier is

\`\`\`
identifier ::= identifier-nondigit (identifier-nondigit | digit)*
identifier-nondigit ::= "_" | "a".."z" | "A".."Z"
digit ::= "0".."9"
\`\`\`

(C11 §6.4.2.1 with UCNs and implementation-defined extended characters
omitted.)  The C4 implementation rejects ISO-8859-1 bytes outside ASCII in
identifiers; accepting them is not required by the assignment and is
classified as valid-C-outside-C4 (may be rejected).  ISO-8859-1 bytes inside
character constants and string literals are source characters and are
accepted (AB.pdf, L-001).

## L-007 — Integer constants

The assignment admits only decimal constants, the constant \`0\`, and
character constants; integer suffixes and encoding prefixes are not
supported (AB.pdf: "we process only decimal-constants, 0 and
character-constants. In this connection we ignore integer-suffixes and
prefixes for the encoding").

\`\`\`
integer-constant ::= "0" | [1-9][0-9]*
\`\`\`

- \`0\` is the only octal constant admitted; \`00\`, \`01\`, \`09\`, and any
  other leading-zero digit sequence are invalid (the first offending
  character is the second digit).
- Hexadecimal constants (\`0x...\`) are invalid; the \`x\` is the first
  offending character.
- Integer suffixes (\`u\`, \`U\`, \`l\`, \`L\`, \`ll\`, \`LL\`) and floating
  constants are invalid; the first character after the digit sequence is the
  first offending character.
- The lexer does not convert the token to an integer (AB.pdf hint).

## L-008 — Character constants

\`\`\`
character-constant ::= "'" c-char "'"
c-char ::= any source character except "'", "\\", or newline
         | simple-escape-sequence
simple-escape-sequence ::= "\\'" | "\\\"" | "\\?" | "\\\\"
                         | "\\a" | "\\b" | "\\f" | "\\n" | "\\r" | "\\t" | "\\v"
\`\`\`

- Exactly one \`c-char\` is required.  \`''\` is an empty character constant and
  \`'ab'\` is a multi-character character constant; both are lexical errors
  (AB.pdf: "character-constants are restricted to single characters").
- Octal-escape-sequences and hexadecimal-escape-sequences are elided
  (AB.pdf), so \`'\0'\`, \`'\012'\`, and \`'\x41'\` are invalid.  Universal
  character names are likewise invalid.
- The token spelling is the exact source text, including the quotes and the
  backslash of an escape sequence; it is not decoded in \`--tokenize\` output.

## L-009 — String literals

\`\`\`
string-literal ::= '"' s-char* '"'
s-char ::= any source character except '"', "\\", or newline
         | simple-escape-sequence
\`\`\`

- Encoding prefixes (\`L\`, \`u\`, \`U\`, \`u8\`) are omitted; \`L"x"\` is an
  identifier \`L\` followed by a string literal, not a prefixed string token.
- Octal and hexadecimal escapes and universal character names are invalid in
  the same way as in character constants.
- A newline before the closing quote is a lexical error.
- Adjacent string literals are **not** concatenated (phase 6 is omitted);
  \`"a" "b"\` is two string-literal tokens.
- The token spelling is the exact source text including quotes and escape
  backslashes; it is not decoded in \`--tokenize\` output.

## L-010 — Punctuators and maximal munch

The punctuators are the C11 §6.4.6 set:

\`\`\`
[ ] ( ) { } . ->
++ -- & * + - ~ !
/ % << >> < > <= >= == != ^ | && ||
? : ; ...
= *= /= %= += -= <<= >>= &= ^= |=
, # ##
<: :> <% %> %: %:%:
\`\`\`

- The lexer uses maximal munch (C11 §6.4p4): at each position it takes the
  longest punctuator that matches.  Examples: \`...\` before \`.\`;
  \`%:%:\` before \`%:\` before \`%\`; \`<<=\` before \`<<\` before \`<\`;
  \`->\` before \`-\`; \`&&\` before \`&\`.
- Digraphs are recognized as their own punctuator tokens and are printed
  with their original spelling by \`--tokenize\`.  The parser and pretty
  printer canonicalize them to \`[ ] { } # ##\` (CD.pdf: "do not print
  digraphs. Print the regular punctuator instead.").
- \`?\` alone is a punctuator; \`?:\` is two punctuators, not one.
- \`#\` and \`##\` are punctuators because preprocessing is absent; the parser
  has no production for them and rejects them.

## L-011 — Maximal munch across token classes

Maximal munch applies to the whole token grammar.  In particular:

- An identifier is extended as far as possible; \`ifx\` is an identifier, not
  \`if\` followed by \`x\`.
- An integer constant is extended over the maximal digit sequence; \`123abc\`
  is an invalid integer constant (suffix \`abc\`), not \`123\` followed by
  \`abc\`.
- \`a+++b\` tokenizes as \`a\`, \`++\`, \`+\`, \`b\`; \`a+++++b\` as \`a\`, \`++\`,
  \`++\`, \`+\`, \`b\`.
- \`....\` tokenizes as \`...\` followed by \`.\`.

## L-012 — \`--tokenize\` output contract

For \`c4 --tokenize FILE\`, each token is printed to stdout on its own line:

\`\`\`
<filename>:<line>:<column>: <kind> <spelling>
\`\`\`

- \`<filename>\` is the input file name exactly as supplied on the command
  line.
- \`<kind>\` is one of \`keyword\`, \`identifier\`, \`constant\`,
  \`string-literal\`, \`punctuator\`.
- \`<spelling>\` is the exact source text of the token.  Escape sequences and
  digraphs are not replaced.
- The internal end-of-file token is not printed (AB.pdf hint: the token
  stream ends with a not-printed end-of-file token).
- On success the process exits 0.

Example from AB.pdf (the source file \`test.c\` has \`42  if\` on line 1 and
\` "bla\\n"x+\` on line 2):

\`\`\`
test.c:1:1: constant 42
test.c:1:5: keyword if
test.c:2:2: string-literal "bla\n"
test.c:2:9: identifier x
test.c:2:10: punctuator +
\`\`\`

## L-013 — Lexical errors

- A file with a lexical error is rejected with exit code 1 and a diagnostic
  on stderr in the form \`<filename>:<line>:<column>: error: <message>\`
  (AB.pdf, Task A).
- The location is the first character that makes the input invalid (AB.pdf,
  Task B).
- Concretely: the offending character for an invalid character; the
  backslash for an invalid escape sequence; the second character of an
  invalid integer prefix/suffix; the opening quote for an unterminated
  character constant or string literal; the start of an unterminated block
  comment.  The last three are interpretations of "first character that
  makes the input invalid" and are recorded in ambiguities.md.

## L-014 — Linear running time

The running time of the lexer must be proportional to the input size
(AB.pdf).  The reference lexer and the conformance corpus are designed so
that this can be checked by input-size scaling; the C++ implementation is
not instrumented.

## Implementation interface

- Lexer: \`src/Lexer.cpp\`, \`src/Lexer.hpp\`.
- Token names and printing: \`src/Token.cpp\`, \`src/TokenKinds.def\`,
  \`src/KeyWords.def\`.
- CLI mode: \`src/Compiler.cpp\` (\`run\`/\`tokenizeFile\`) and \`src/main.cpp\`.

## Verification

The executable reference lexer is \`verification/conformance/c4ref/lexer.py\`.
It is compared token-for-token with \`c4 --tokenize\` by
\`python3 verification/conformance/run_conformance.py --suite lexer\` over
generated and boundary corpora.  The requirement-to-test mapping is in
requirements.yaml and the executed evidence is in
\`verification/conformance/results/evidence.json\`.

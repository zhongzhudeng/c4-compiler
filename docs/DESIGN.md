# C4 compiler design, implementation and verification

Status: implementation complete (assignments A-G).  This document is the
durable design record for the repository; update it when the architecture
changes.

## Objective

Complete the Saarland University C4 compiler project (assignments A-G) in this
repository.  Required CLI modes: --tokenize, --parse, --print-ast, --compile,
--optimize; accepted programs exit 0, rejected programs exit 1 and print
<filename>:<line>:<column>: error: <message>.  --compile and --optimize write
human-readable LLVM IR to <basename>.ll in the current directory (input
foo/bar.c produces bar.ll).

Host: macOS arm64.  Toolchain: Apple clang 21, CMake 4.4.5, Homebrew LLVM
23.1.5 at /opt/homebrew/opt/llvm (discovered through "brew --prefix llvm",
never a hard-coded Cellar path), Catch2 3.16.1.  No LLVM 15 was built.

## Original implementation assessment

Recovered repository: /Users/dzz/projects/c4, branch "decl", commit 8eb4cd1
plus an uncommitted, non-building parser/AST refactor.  Checkpoint commit
81c8655 preserves that state; the pre-redesign tree is also backed up under
/tmp/c4-original-*.tar.gz.

What worked and was preserved:
- The hand-written lexer recognized most C11 tokens, keywords, comments,
  strings, character constants, CR/LF/CRLF positions, and printed the required
  file:line:column: kind spelling token format.
- Symbol interning (SymbolInternalizer) and the .def keyword/token tables.

Lexer defects demonstrated on the original code (all fixed):
- // and /* */ comments at EOF were rejected as errors.
- 00 and 012 were accepted as constants (octal constants are outside C4).
- 42u, 42abc, 0x10, 1.0 were split into multiple tokens instead of being
  rejected as invalid constants.
- Token kinds for #, %=, %>, and | were wrong.
- Diagnostics printed an extra "error:"; invalid escapes were reported at the
  start of the literal instead of at the first invalid character.

Why the old AST obstructed development (established from the source):
- Every child was std::unique_ptr, so parser actions moved nodes through
  ActOn... callbacks and returned new owners.  Semantic actions determined AST
  lifetimes.
- Nodes stored const Decl&/const LabelDecl* references while the AST was still
  being built; labels were resolved through a back-list of references to nodes
  owned elsewhere, which is fragile under vector growth.
- Expr/Stmt/Decl used parallel hand-maintained hierarchies plus .def ranges;
  several declarations and definitions disagreed, so the worktree did not
  compile (NamedDeclarator/ValuedDeclarator, StructSpec,
  LabelDecl::getIdentifier, Function vs FunctionDecl).
- const_cast was pervasive and ownership transfer happened inside semantic
  actions, so later passes depended on AST internals.

Preserved: lexer architecture and behavior (fixed in place), token/keyword
tables, symbol interning, CLI contract.  Replaced: parser, AST, semantic
analysis, pretty printer, build files, tests.

## Final architecture

    source bytes
      -> Lexer (tokens incl. internal EOF)
      -> Parser (syntax-only AST)
      -> AST (vector-owned, typed 32-bit NodeIds, std::variant kinds)
      -> Sema (name resolution + type checking -> SemanticModel)
      -> Printer (AST + SemanticModel)
      -> CodeGen (AST + SemanticModel + TypeLayout -> LLVM 23 IR)
      -> [--optimize only] mem2reg -> own SCCP
      -> verified, human-readable .ll

Source files:

    src/Support.hpp        SourceLoc, CompileError, Diagnostics
    src/Token.*            token kinds, spelling, tokenKindName
    src/Lexer.*            maximal-munch lexer, comments, constants, escapes
    src/SymbolInternalizer.*  string interning (Symbol = const std::string*)
    src/AST.*              vector-owned AST, typed IDs, declarator queries
    src/Type.*             interned types, TypeLayout over llvm::DataLayout
    src/SemanticModel.*    independent per-node semantic results
    src/Parser.*           recursive-descent / precedence-climbing parser
    src/Sema.*             scopes, declarators, type checking, labels
    src/Printer.*          reference pretty printer (task D format)
    src/CodeGen.*          LLVM 23 IR construction
    src/Optimizer.*        mem2reg only (permitted SSA construction)
    src/SCCP.*             own sparse conditional constant propagation
    src/Compiler.*         pipeline driver and .ll output
    src/main.cpp           CLI

### AST ownership and node relationships

- One AST object owns all nodes by value in std::vectors: exprs_, stmts_,
  decls_, declarators_, typeSpecs_, labels_.
- Nodes refer to each other only through typed IDs: ExprId, StmtId, DeclId,
  DeclaratorId, TypeSpecId, LabelId, each a 32-bit index with an invalid
  sentinel.
- Node kinds are std::variant alternatives (ExprNode, StmtNode, DeclNode,
  DeclaratorNode).  The source location lives in the wrapper (Expr, Stmt,
  Decl, Declarator).
- No code holds pointers/references into AST vectors across an insertion.
  Parsing finishes before Sema/CodeGen, so later phases obtain short-lived
  references through ast.expr(id) etc.
- AST nodes contain no resolved types, symbols, or semantic pointers.
- If profiling showed variant size/branch overhead mattered, split pools by
  node category or use a bump allocator; the ID-based interface stays.

### SemanticModel

Independent from AST ownership, constructed after parsing and sized from AST
counts:
- ExprInfo: type, value category, null-pointer-constant flag, resolved DeclId,
  member record/index, goto label, constant value, decoded string bytes.
- DeclInfo: type, name, definition/global/tentative flags, function parameter
  DeclIds, struct record for tags.
- DeclaratorInfo: resolved type and the parameter list of the outer function
  declarator.
- TypeSpecInfo: resolved StructDecl*.
- LabelInfo and per-StmtId goto targets.
- Owns StructDecl records (tag, fields with types/offsets, completeness).

Scopes and symbol tables exist only during Sema.  Labels are function-scoped
and resolved after the function body so forward goto works.

### Type interning

TypeContext interns canonical Type objects; type equality is pointer equality.
Type supports void, char, int, pointer, function, struct, and an array type
used only for string literals (C4 has no array declarators).  Struct types
point to a mutable StructDecl, enabling recursive/incomplete types.
TypeLayout maps types to LLVM types and uses the host llvm::DataLayout for
sizes, alignment, and field offsets; Sema and CodeGen share it, so sizeof
cannot drift from the generated layout.

### Diagnostics

Diagnostics::error is the only error formatter and always throws CompileError,
which main turns into exit code 1.  The lexer reports the first invalid
character; the parser reports the first token that cannot continue the
grammar (or the EOF position for a missing token); Sema uses the first terminal
of the erroneous construct (operator for binary errors, keyword for
if/while/return, identifier for names).

### Parser (task C)

Syntax-only; it discards source parentheses but keeps a fully parenthesizable
structure.  It implements the restricted C4 grammar: identifiers, constants,
string literals, (), [], calls, ., ->, sizeof, unary & * - ! +, binary
* + - < == != && ||, ?:, =, declarations without initializers,
pointer/parenthesized/function declarators, abstract declarators for sizeof,
and labeled/compound/expression/if/if-else/while/break/continue/return/goto
statements.  Local function declarations are allowed; function definitions are
file-scope only.  Declarations are classified syntactically as
VarDecl/FunctionDecl/TagDecl from declarator shape.

### Sema (task E)

Implements the C4 semantics: ordinary/tag/label namespaces, lexical scopes,
canonical function declarations by name, tentative file-scope object
definitions, recursive/incomplete structs, pointer and integer checks, labels
and goto resolution, break/continue loop checking, and first-terminal error
locations.  Struct parameters, struct returns and struct assignment are
rejected (explicitly permitted by task E); anonymous structs are supported.

### Pretty printer (task D)

Prints from the AST, never from source text.  Formatting follows the reference
pretty.c: tabs for indentation, one blank line between file-scope
declarations, fully parenthesized expressions and declarators, labels at
column zero, sizeof expr printed as (sizeof expr) and sizeof(type) as
(sizeof(type)).  The reference fixture tests/fixtures/pretty.c is reproduced
byte-exactly.

### Code generation (task F)

LLVM 23 opaque-pointer API with IRBuilder.  Mapping:
- char -> i8 (signed), int -> i32, pointers -> ptr, structs -> literal LLVM
  structs from TypeLayout, functions -> llvm::FunctionType.
- File-scope objects become zero-initialized LLVM globals; tentative
  definitions (int g; int g;) share one global by name.
- Every local and parameter is allocated in the entry block and initialized
  with a store, which makes mem2reg applicable.  Locals without initializers
  keep C's indeterminate value.
- char values are sign-extended to i32 for arithmetic and truncated on
  assignment/parameter passing; int arithmetic wraps in 32 bits.
- Struct member access uses getelementptr with the shared struct layout; ->
  dereferences the pointer base; . uses the lvalue address.
- Arrays exist only for string literals: a private constant [N x i8] global,
  decaying to ptr.  Pointer arithmetic and subscripts use typed GEPs; pointer
  difference is computed in i64 and divided by the element size.
- Control flow uses explicit basic blocks: if/else, while, ?:, &&, || with
  short-circuit PHIs, labels/goto (label blocks are pre-created so forward
  gotos work), break/continue.
- Void functions emit ret void; non-void functions without a terminator get a
  zero return (matching main's implicit return).
- llvm::verifyModule runs after generation; invalid IR is an internal error.

### SCCP (task G)

Own implementation in src/SCCP.*, operating on the SSA form produced by
mem2reg.  No LLVM optimization pass other than mem2reg is used; LLVM's SCCP
pass is not referenced.

Analysis:
- Lattice Undefined < Constant(ConstantInt) < Overdefined.
- Executable basic blocks and executable edges.
- Monotone fixed-point sweep over executable blocks; values only move up the
  lattice, so termination is guaranteed.
- Instructions evaluated: binary operators (add/sub/mul/div/rem/and/or/xor/
  shifts, with div-by-zero and undefined-shift guards), icmp, integer casts
  (trunc/zext/sext/bitcast), select, and PHI nodes (meet over executable
  incoming edges).  Loads, calls, allocas and GEPs are conservatively
  Overdefined.
- Conditional branches with a constant condition mark only the taken edge;
  Overdefined conditions mark both edges; Undefined conditions mark none until
  more information arrives.

Transform:
1. Replace every constant-lattice instruction's uses with the constant.
2. Rewrite constant conditional branches as unconditional branches.
3. Recompute CFG reachability from the entry block.
4. Remove PHI operands whose incoming block is unreachable or is no longer a
   predecessor.
5. Replace dead blocks' terminators with unreachable and delete them.
6. Iteratively delete trivially dead instructions (unused, no side effects).
7. verifyModule runs on the result (driver and tests).

Evidence (constant-branch program):

    int main(void) { int x; x = 1; if (1) { x = 2; } else { x = 3; }
                     while (0) { x = 4; } return x; }

--optimize output contains "ret i32 2", has no icmp, no if.else and no
while.body; the deleted branches and blocks are gone.

## Invariants

1. AST nodes are syntax-only; semantic results never extend node lifetime.
2. Node relationships use typed IDs, never owning pointers or cross-node
   references.
3. TypeContext pointer equality is the definition of type compatibility.
4. Diagnostics::error is the only error printer and always throws.
5. Every pass must leave a verifier-clean module.
6. Lexer output spelling is preserved exactly (escapes, digraphs).
7. --compile never runs optimization; --optimize runs only mem2reg and our
   SCCP.

## Build and test

    make                 # configure + build build/debug/c4
    make test            # CTest: Catch2 unit tests + e2e/differential suite
    cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build/debug
    ctest --test-dir build/debug --output-on-failure

The build discovers Homebrew LLVM and Catch2 through "brew --prefix", links
libLLVM-23.dylib, and links the tests against Catch2 3.

## Verification

- 46 Catch2 test cases (lexer, parser, Sema, printer, codegen, SCCP), all
  passing.
- End-to-end suite tests/e2e/run_e2e.py: 17 accepted programs x
  {--compile, --optimize} plus 7 rejected programs; each accepted program is
  compiled to .ll, linked with clang, executed, and compared against its
  expected exit status and stdout; the original C source is also compiled with
  clang and checked differentially.  --print-ast output is re-parsed to check
  the printer/parser round trip.  Rejected programs must exit 1, print the
  required diagnostic format, and produce no .ll.
- Manual probes verified token positions/format, the reference printer
  fixture, pointer/struct/function-pointer edge cases, and SCCP IR.

## Progress

- [x] Recover repository, backup/checkpoint, assess original lexer and AST.
- [x] Fixed lexer (comments at EOF, constants, token kinds, diagnostics, CRLF,
      digraphs).
- [x] Vector/variant AST with typed NodeIds and independent SemanticModel.
- [x] Type interning and shared TypeLayout.
- [x] Parser and parser tests.
- [x] Sema and acceptance/rejection matrix.
- [x] Printer, byte-exact reference fixture, printer tests.
- [x] Modern CMake/Makefile build with Homebrew LLVM 23 and Catch2.
- [x] LLVM 23 code generation and --compile.
- [x] Own SCCP and --optimize (mem2reg only as permitted).
- [x] End-to-end and differential tests.
- [x] Full CTest verification.

## Known limitations (within the assignment scope)

- No preprocessor, no full C11, no arrays declared by the source language, no
  initializers, no for/do/switch/unions/enums/typedefs (outside C4).
- Struct parameters, struct returns and struct assignment are rejected, as
  task E permits.
- --optimize intentionally keeps single-input PHIs and trivially redundant
  blocks; the required SCCP transformations (constant substitution, branch
  folding, dead instruction removal, unreachable block removal) are performed.

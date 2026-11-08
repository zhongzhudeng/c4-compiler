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
    src/Type.*             interned algebraic types, TypeLayout over DataLayout
    src/SemanticModel.*    entities, scopes, and per-node semantic results
    src/Parser.*           recursive-descent / precedence-climbing parser
    src/Sema.*             scopes, declarators, type checking, labels
    src/Printer.*          reference pretty printer (task D format)
    src/CodeGen.*          module-wide IR and per-function lowering
    src/Optimizer.*        mem2reg only (permitted SSA construction)
    src/SCCP.*             own sparse conditional constant propagation
    src/Compiler.*         pipeline phases, target environment, .ll output
    src/main.cpp           CLI

### Pipeline phases and resource ownership

The orchestration layer is a sequence of value-returning phase functions rather
than a mutable Compiler object:

    TokenizedSource tokenizeFile/tokenizeString(...)
    ParsedProgram   parse(TokenizedSource)
    AnalyzedProgram analyze(ParsedProgram, TypeContext&, TypeLayout&)
    llvm::Module    generateModule(const AnalyzedProgram&, TargetEnvironment&, name)
    void            optimizeModule(llvm::Module&)
    void            writeModule(const llvm::Module&, path, sourceFilename)
    int             run(filename, CompileMode)

Each result owns exactly the state its stage produced and nothing later stages
do not need:

- TokenizedSource owns the token vector and the SymbolInternalizer its token
  text and filenames point at.  Tokenization needs no parser, Sema, or backend.
- ParsedProgram owns the SymbolInternalizer and the completed AST.  The parser
  consumes the token vector, so tokens are destroyed with the input stage; AST
  names remain valid because symbol ownership moved forward.
- AnalyzedProgram owns the ParsedProgram and the SemanticModel.  It borrows
  TypeContext and TypeLayout because those are shared with CodeGen.  Its
  constructor is private and only analyze() can create it, so a model that Sema
  has not completed cannot satisfy a later stage's requirement.
- TargetEnvironment owns the LLVMContext, DataLayout, and TypeLayout, and the
  target triple.  Its constructor performs the one-time LLVM target
  registration, derives the DataLayout from a generic-CPU target machine (with
  the established fallback layout), and builds the TypeLayout over the same
  context and layout.  It is constructed only for --compile/--optimize, so
  frontend-only modes never register targets or create a TargetMachine.
- generateModule creates the llvm::Module in the TargetEnvironment's context,
  sets its triple and data layout from the same environment, and lowers the
  AnalyzedProgram.  Sema and CodeGen therefore cannot observe different layout
  semantics.

run() composes the stages for each mode:

    --tokenize   tokenizeFile -> print tokens (internal EOF skipped)
    --parse      tokenizeFile -> parse -> analyze
    --print-ast  tokenizeFile -> parse -> analyze -> Printer
    --compile    tokenizeFile -> parse -> TargetEnvironment -> analyze
                 -> generateModule -> writeModule
    --optimize   same, then optimizeModule before writeModule
    default      CompileMode::Compile

Frontend-only modes build a local FrontendEnvironment (LLVMContext,
DataLayout(""), TypeLayout) for Sema's sizeof handling.  In compile modes the
TargetEnvironment is declared before TypeContext and AnalyzedProgram, and the
module after them, so reverse declaration order destroys the module first, then
the analyzed program, then the borrowed types/layout, then the environment that
owns the context and layout.  The same ordering lets a CompileError from any
phase unwind without returning a partially analyzed stage result.

### AST ownership and node relationships

- One AST object owns all nodes by value in std::vectors: exprs_, stmts_,
  decls_, declarators_, typeSpecs_, labels_.
- Nodes refer to each other only through typed IDs: ExprId, StmtId, DeclId,
  DeclaratorId, TypeSpecId, LabelId, each a 32-bit index with an invalid
  sentinel.
- Node kinds are std::variant alternatives (ExprNode, StmtNode, DeclNode,
  DeclaratorNode).  The source location lives in the wrapper (Expr, Stmt,
  Decl, Declarator).
- TypeSpec is a variant of Void/Char/Int/Struct, and SizeofExpr is a variant
  of an expression operand or a type-name form.  Struct-only tag/field data
  exists only on TypeSpec::Struct, and neither sizeof form can carry the
  other form's payload.
- No code holds pointers/references into AST vectors across an insertion.
  Parsing finishes before Sema/CodeGen, so later phases obtain short-lived
  references through ast.expr(id) etc.
- AST nodes contain no resolved types, symbols, or semantic pointers.
- If profiling showed variant size/branch overhead mattered, split pools by
  node category or use a bump allocator; the ID-based interface stays.

### Semantic identities: DeclId, EntityId, ScopeId

Three identities are kept distinct:

- DeclId (AST.hpp) identifies a syntactic declaration node.  It never stands
  in for a symbol.
- EntityId (SemanticModel.hpp) identifies a declared object or function.  An
  entity owns its canonical type, linkage, definition state, first source
  location, the declarations that denote it, and (for a function definition)
  its parameter declarations.  Compatible redeclarations and block-scope
  declarations of an external function point at one entity.
- ScopeId (SemanticModel.hpp) identifies a lexical scope.  Scopes are owned by
  the model in stable storage, so an ID does not depend on a mutable scope
  stack position.  Each scope has a kind (file, block, function prototype,
  function), an explicit parent, and namespace-specific bindings: ordinary
  identifiers bind to entities, tags bind to StructDecl records, and function
  scopes also bind labels.  Members are not lexical bindings; they remain
  StructDecl::fields.

External linkage determines entity identity.  File-scope objects and all
functions are external; block-scope objects and parameters have no linkage.
Sema keeps a translation-unit index of external entities so a block-scope
function declaration and a later file-scope definition resolve to the same
entity, while the block binding itself stays local to its scope.

### SemanticModel

Independent from AST ownership, constructed after parsing and sized from AST
counts:
- ExprInfo: type, value category, null-pointer-constant flag, resolved
  EntityId, member record/index, goto label, constant value, decoded string
  bytes.
- DeclInfo: EntityId for an ordinary declaration, the ScopeId of the binding
  introduced by that declaration, and StructDecl* for a tag declaration.
- Entity storage: canonical type, kind, linkage, definition state, first
  location, declarations, and definition parameters.
- Scope storage: kind, parent, and ordinary/tag/label bindings.
- TypeSpecInfo: resolved StructDecl*.
- LabelInfo and per-StmtId goto targets.
- Owns StructDecl records (tag, fields with types/offsets, completeness).

A function definition adopts the prototype scope of its defining declarator
as the function scope: parameter bindings and parameter-list tags remain
visible in the body, the scope is reparented to the declaration's enclosing
scope, and labels are bound there.  Prototype scopes of non-defining or
nested declarators remain dead and cannot leak names into the body.

### Type representation and interning

Type is a discriminated union (std::variant) with one alternative per C4
category:

- Void, Char, Int: no payload.
- Pointer{pointee}.
- Function{result, params}.
- Struct{record}: a semantic StructDecl*, not a tag spelling.
- Array{element, count}: used only for string literals (C4 has no array
  declarators).

There is no separate Kind field.  Category-specific data is reached through
checked accessors (pointee(), element(), count(), result(), params(),
record()), each of which uses std::get and therefore fails loudly on a
mismatched access.  is<T>()/getIf<T>() provide category inspection; the
static predicates (isInteger, isPointer, isFunction, isObject, ...) are thin
queries over the variant.  Type objects are immutable once interned.

TypeContext remains the canonical interner: structurally identical pointer,
function, and array types share one Type object; struct types are keyed by
their StructDecl identity; primitive types are singletons.  Type objects are
owned by TypeContext in stable heap storage, so const Type* is a stable
canonical handle.  Incomplete and completed references to the same struct
retain the same Type, recursive structs through pointers work, and shadowed
tags have distinct identities.

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

Implements the C4 semantics through explicit declaration operations: file-
scope and block-scope objects, function declarations and definitions,
compatible redeclarations, external-linkage identity, tentative definitions,
struct tags/forward declarations/completion, parameters, labels, recursive
and incomplete structs, pointer and integer checks, labels and goto
resolution, break/continue loop checking, and first-terminal error locations.
Declarations bind names into a ScopeId; compatible declarations reuse an
EntityId; a block-scope function declaration creates a local binding for the
external entity.  Struct parameters, struct returns and struct assignment are
rejected (explicitly permitted by task E); anonymous structs are supported.

### Pretty printer (task D)

Prints from the AST, never from source text.  Formatting follows the reference
pretty.c: tabs for indentation, one blank line between file-scope
declarations, fully parenthesized expressions and declarators, labels at
column zero, sizeof expr printed as (sizeof expr) and sizeof(type) as
(sizeof(type)).  The reference fixture tests/fixtures/pretty.c is reproduced
byte-exactly.

### Code generation (task F)

LLVM 23 opaque-pointer API with IRBuilder.  CodeGen is split into module-wide
and function-scoped responsibilities:

- CodeGen owns the llvm::Module and LLVMContext, the shared TypeLayout and
  TypeContext, the EntityId maps for external objects and functions, and
  module-wide string constants.  It creates external declarations once and
  verifies the module after all definitions.
- CodeGen::FunctionEmitter is a nested class constructed for exactly one
  function definition and destroyed afterwards.  It owns the current
  llvm::Function, its return type, the IRBuilder and alloca builder, the local
  EntityId-to-alloca map, label blocks, and loop contexts.  There is no
  function state on the module-level object and no manual reset between
  definitions.

Expression lowering distinguishes four result categories:

- Place{address, type}: an addressable object, including string-literal
  arrays.  Assignment targets and member bases require this category.
- RValue{value, type}: a computed scalar (integer or pointer) value.  Void,
  struct, array, and function values are deliberately not representable.
- FunctionDesignator{callee, function type}: a callable function or function
  pointer.  Direct calls use the function; indirect calls use the pointer.
- VoidResult: evaluation with no resulting value, produced by void calls and
  void conditional expressions.

Conversions are explicit: emitValue applies lvalue-to-rvalue conversion,
array-to-pointer decay, and function-to-pointer decay; load performs a scalar
load; convert handles integer promotion/truncation, pointer compatibility, and
null pointer constants using Sema's nullPointerConstant flag; emitDiscarded
evaluates an expression statement, applying scalar lvalue conversion without
requiring a value for arrays or structs.  Sema's value categories and the
canonical TypeContext types are authoritative; CodeGen only checks LLVM
lowering invariants.

The LLVM mapping is otherwise unchanged:
- char -> i8 (signed), int -> i32, pointers -> ptr, structs -> literal LLVM
  structs from TypeLayout, functions -> llvm::FunctionType.
- File-scope objects become zero-initialized LLVM globals; tentative
  definitions (int g; int g;) share one global by EntityId.
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
  short-circuit PHIs; PHI incoming blocks are recorded after each arm so
  nested expressions keep predecessor dominance.  Labels/goto use pre-created
  label blocks so forward gotos work; break/continue use the innermost loop
  context.
- Void functions emit ret void; non-void functions without a terminator get a
  zero return (matching main's implicit return).
- Missing local storage, missing labels or entities, invalid conversions, and
  invalid insertion points are internal errors: they throw
  std::runtime_error("codegen: ..."), which main reports as
  "c4: internal error: ..." with exit code 2.  CodeGen never synthesizes a
  global for a local entity and never emits source-language diagnostics.
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
8. DeclId is syntax, EntityId is semantic identity, and ScopeId is lexical
   scope; they are never used interchangeably.
9. Multiple DeclIds may denote one EntityId; each binding belongs to a
   defined ScopeId.
10. External linkage, not the syntactic declaration site, determines entity
    identity.
11. CodeGen expression results distinguish Place, RValue, FunctionDesignator,
    and VoidResult; a void expression is never an RValue.
12. A local EntityId resolves only to its own local storage; missing storage is
    an internal error, never a synthesized global.
13. FunctionEmitter state has exactly one function-definition lifetime; no
    module-level mutable function state exists.
14. Type categories are variant alternatives; category data is reachable only
    through checked accessors, and canonical Type* identity is preserved.
15. TypeSpec and SizeofExpr encode their mutually exclusive syntactic forms
    directly; an absent abstract declarator remains representable.
16. Stage results own the symbols and AST they expose; token and AST data are
    moved forward, never duplicated.
17. AnalyzedProgram exists only after Sema completes, and CodeGen consumes it;
    a partially analyzed model cannot satisfy a later stage.
18. Frontend-only modes construct no TargetEnvironment and perform no target
    registration.

## Build and test

    make                 # configure + build build/debug/c4
    make test            # CTest: Catch2 unit tests + e2e/differential suite
    cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build/debug
    ctest --test-dir build/debug --output-on-failure

The build discovers Homebrew LLVM and Catch2 through "brew --prefix", links
libLLVM-23.dylib, and links the tests against Catch2 3.

## Verification

- 95 Catch2 test cases (lexer, parser, Sema, types/AST, printer, codegen,
  SCCP, pipeline) plus the end-to-end harness (96 CTest tests), all passing in
  Debug, Release, and AddressSanitizer builds.
  Nine Sema cases are architectural regressions for entity identity, binding
  identity, and scope lifetime; eleven CodeGen cases cover typed expression
  results, missing local storage, conversions, calls, control flow, and
  function-state isolation; eight Type/AST cases cover canonical
  primitive/pointer/function/array/struct identity, incomplete-to-complete
  struct identity, predicates and printing, TypeLayout sizes and offsets, both
  sizeof forms, TypeSpec alternatives, and checked category access.
  Eleven pipeline cases cover token/symbol ownership, parse-before-Sema
  separation, the analyzed-program prerequisite, target-free frontend
  execution, failure behavior, independent invocations, per-mode stage
  composition, output naming, and default/--compile equivalence.
- End-to-end suite tests/e2e/run_e2e.py: 23 accepted programs x
  {--compile, --optimize} plus 14 rejected programs (430 checks); each
  accepted program is compiled to .ll, linked with clang, executed, and
  compared against its expected exit status and stdout; the original C source
  is also compiled with clang and checked differentially.  --print-ast output
  is re-parsed to check
  the printer/parser round trip.  Rejected programs must exit 1, print the
  required diagnostic format, and produce no .ll.
- Full A-G conformance harness: 726/726 cases passed, including 108/109
  grammar alternatives (the uncovered alternative is
  struct-declaration/without-declarator-list).
- LLVM verifier checks pass for generated IR in both --compile and --optimize;
  the conformance suite also runs llvm-as and executes both modes.
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
- [x] CodeGen redesign: typed expression results and function-scoped emitters.
- [x] Algebraic Type representation and AST invariant cleanup.
- [x] Phase-oriented pipeline with explicit ownership and target-free frontend
      modes.

## Known limitations (within the assignment scope)

- No preprocessor, no full C11, no arrays declared by the source language, no
  initializers, no for/do/switch/unions/enums/typedefs (outside C4).
- Struct parameters, struct returns and struct assignment are rejected, as
  task E permits.
- --optimize intentionally keeps single-input PHIs and trivially redundant
  blocks; the required SCCP transformations (constant substitution, branch
  folding, dead instruction removal, unreachable block removal) are performed.

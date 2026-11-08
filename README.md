# C4 Compiler

A from-scratch compiler for C4 — the restricted C subset from the Saarland
University compiler course — targeting LLVM 23. It implements the complete
pipeline: lexing, parsing, semantic analysis, pretty-printing, LLVM IR
generation, and an independent optimizer.

## Pipeline

```
source bytes
  -> Lexer      tokens (including internal EOF)
  -> Parser     syntax-only AST
  -> Sema       name resolution and type checking -> SemanticModel
  -> Printer    pretty-printed source (--print-ast)
  -> CodeGen    LLVM 23 IR
  -> Optimizer  mem2reg + own SCCP (--optimize only)
  -> .ll        verified, human-readable LLVM IR
```

- **Lexer** — hand-written maximal-munch lexer for C11 tokens, comments, and
  string/character escapes, with CR/LF/CRLF source positions.
- **AST** — nodes are owned in vectors and referenced by typed 32-bit IDs; the
  AST stores syntax only.
- **Parser** — recursive descent with precedence climbing for the C4 grammar.
- **Sema** — scopes, declarators, type checking, structs, labels/goto, and
  tentative definitions. Results live in an independent SemanticModel.
- **CodeGen** — LLVM 23 opaque-pointer API with module-wide declarations and a
  per-function emitter; expression results are typed as Place, RValue,
  FunctionDesignator, or Void. Sema and CodeGen share TypeLayout, so sizeof
  cannot drift from the generated layout.
- **SCCP** — the project's own sparse conditional constant propagation;
  mem2reg is the only LLVM pass used.

The architecture, invariants, and verification record are documented in
[docs/DESIGN.md](docs/DESIGN.md).

## Requirements

- CMake 3.20+, Ninja, and a C++20 compiler
- LLVM 23 development files (find_package(LLVM CONFIG))
- Catch2 3
- Python 3 and clang for the end-to-end suite

On macOS with Homebrew:

```
brew install llvm catch2 cmake ninja python
```

CMake discovers Homebrew LLVM and Catch2 through brew --prefix. For other
installations, point CMAKE_PREFIX_PATH at their prefixes.

## Build

```
make            # configure + build build/debug/c4
make test       # build, then run the full CTest suite
make release    # Release build in build/release
```

Or configure and build manually:

```
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

## Usage

```
c4 --tokenize <file.c>    print the token stream
c4 --parse <file.c>       parse and semantically analyze
c4 --print-ast <file.c>   pretty-print the program to stdout
c4 --compile <file.c>     emit LLVM IR to <basename>.ll
c4 --optimize <file.c>    emit optimized LLVM IR to <basename>.ll
c4 <file.c>               same as --compile
c4 --help
```

Accepted programs exit 0. Rejected programs print
<file>:<line>:<column>: error: <message> and exit 1; internal failures exit 2.
--compile and --optimize write <basename>.ll to the current directory, so
foo/bar.c produces bar.ll.

For example:

```
$ ./build/debug/c4 --compile tests/e2e/programs/arith.c
$ clang arith.ll -o arith && ./arith
$ echo $?
46
```

## Tests

make test runs:

- 54 Catch2 test cases covering the lexer, parser, Sema, pretty printer, code
  generation, and SCCP.
- An end-to-end e2e CTest that compiles 20 accepted programs with both
  --compile and --optimize, links and runs them, compares the results with
  clang, checks --print-ast round trips, and verifies that 10 invalid programs
  are rejected with the required diagnostic format and no .ll output.

The e2e test is only registered when Python 3 and clang are available at
configure time.

## Repository layout

```
CMakeLists.txt    build definition
Makefile          configure/build/test convenience targets
src/              compiler sources
tests/            Catch2 unit tests, e2e suite, fixtures
docs/DESIGN.md    architecture, invariants, verification record
```

## Language scope

C4 is deliberately restricted: no preprocessor, no source-declared arrays, no
initializers, and no for/do/switch/unions/enums/typedefs. Struct parameters,
struct returns, and struct assignment are rejected. See
[docs/DESIGN.md](docs/DESIGN.md) for the complete list.

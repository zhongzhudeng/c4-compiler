add_library(compiler SHARED
  Compiler.cpp
  Diagnostic.cpp
  Lexer.cpp
  ParseDecl.cpp
  ParseExpr.cpp
  Parser.cpp
  ParseStmt.cpp
  Printer.cpp
  Scope.cpp
  Sema.cpp
  SymbolInternalizer.cpp
  SyntaxDecl.cpp
  SyntaxExpr.cpp
  SyntaxStmt.cpp
  Type.cpp
  Token.cpp
)
target_compile_features(compiler PRIVATE cxx_std_20)

if(EXISTS "llvm/install/bin/llvm-config")
  target_compile_options(compiler PRIVATE -Wall -W ${LLVM_CFLAGS})
else()
  target_compile_options(compiler PRIVATE -Wall -W)
endif()

add_executable(c4 main.cpp)
target_compile_features(c4 PRIVATE cxx_std_20)

if(EXISTS "llvm/install/bin/llvm-config")
  target_compile_options(c4 PRIVATE -Wall -W ${LLVM_CFLAGS})
  target_link_options(c4 PRIVATE "-Wl,--search_paths_first" "-Wl,-headerpad_max_install_names")
  target_link_directories(c4 PRIVATE ${LLVM_LIBDIR} /opt/homebrew/opt/zstd/lib)
  target_link_libraries(c4 PRIVATE compiler ${LLVM_LIBS} ${SYSTEM_LIBS})
else()
  target_compile_options(c4 PRIVATE -Wall -W)
  target_link_libraries(c4 PRIVATE compiler)
endif()
add_library(compiler Diagnostic.cpp Lexer.cpp SymbolInternalizer.cpp Token.cpp)
target_compile_features(compiler PUBLIC cxx_std_17)

add_executable(main main.cpp)
target_compile_features(main PRIVATE cxx_std_17)
target_link_libraries(main PRIVATE compiler)
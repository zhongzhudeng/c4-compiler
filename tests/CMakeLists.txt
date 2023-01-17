include(CTest)
enable_testing()

find_package(Catch2 3 REQUIRED)
find_package(fmt 9 REQUIRED)
add_executable(tests Lexer.cpp Parser.cpp)

target_include_directories(tests PRIVATE ../src)
target_compile_features(tests PRIVATE cxx_std_20)
target_compile_options(tests PRIVATE -Dprivate=public -Wall -W ${LLVM_CFLAGS})
target_link_options(tests PRIVATE "-Wl,-search_paths_first" "-Wl,-headerpad_max_install_names")
target_link_directories(tests PRIVATE ${LLVM_LIBDIR} /opt/homebrew/opt/zstd/lib)
target_link_libraries(tests PRIVATE fmt::fmt compiler Catch2::Catch2WithMain ${LLVM_LIBS} ${SYSTEM_LIBS})

include(Catch)
catch_discover_tests(tests)
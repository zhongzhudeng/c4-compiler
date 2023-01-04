cmake_minimum_required(VERSION 3.0.0)
project(
  c4
  VERSION 0.1.0
  LANGUAGES CXX
)

exec_program("llvm/install/bin/llvm-config"
  ${CMAKE_CURRENT_SOURCE_DIR}
  ARGS "--cppflags"
  OUTPUT_VARIABLE LLVM_CFLAGS
)

exec_program("llvm/install/bin/llvm-config"
  ${CMAKE_CURRENT_SOURCE_DIR}
  ARGS "--libdir"
  OUTPUT_VARIABLE LLVM_LIBDIR
)

exec_program("llvm/install/bin/llvm-config"
  ${CMAKE_CURRENT_SOURCE_DIR}
  ARGS "--libs"
  OUTPUT_VARIABLE LLVM_LIBS
)

exec_program("llvm/install/bin/llvm-config"
  ${CMAKE_CURRENT_SOURCE_DIR}
  ARGS "--system-libs"
  OUTPUT_VARIABLE SYSTEM_LIBS
)

file(GLOB src_files "src/*.cpp")
add_executable(c4 ${src_files})
target_compile_features(c4 PRIVATE cxx_std_17)
target_compile_options(c4 PRIVATE -Wall -W ${LLVM_CFLAGS})
target_link_options(c4 PRIVATE "-Wl,-search_paths_first" "-Wl,-headerpad_max_install_names")
target_link_directories(c4 PRIVATE ${LLVM_LIBDIR} /opt/homebrew/opt/zstd/lib)
target_link_libraries(c4 PRIVATE ${LLVM_LIBS} ${SYSTEM_LIBS})

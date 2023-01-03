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
  ARGS "--ldflags --libs --system-libs"
  OUTPUT_VARIABLE LLVM_LDFLAGS
)

set(CMAKE_COMPILE_FLAGS)

file(GLOB src_files "src/*.cpp")
add_executable(c4 ${src_files})
target_compile_features(c4 PRIVATE cxx_std_17)
target_compile_options(c4 PRIVATE -Wall -W ${LLVM_CFLAGS})
target_link_options(c4 PRIVATE ${LLVM_LDFLAGS})

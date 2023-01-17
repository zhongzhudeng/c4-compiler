cmake_minimum_required(VERSION 3.0.0)
project(
  c4
  VERSION 0.1.0
  LANGUAGES CXX
)

if(EXISTS "llvm/install/bin/llvm-config")
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
endif()

add_subdirectory(src)
add_subdirectory(tests)

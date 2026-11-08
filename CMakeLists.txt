cmake_minimum_required(VERSION 3.20)
project(c4 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Discover Homebrew prefixes instead of hard-coding Cellar paths.
find_program(BREW_EXECUTABLE brew)
if(BREW_EXECUTABLE)
  execute_process(
    COMMAND ${BREW_EXECUTABLE} --prefix llvm
    OUTPUT_VARIABLE HOMEBREW_LLVM_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  execute_process(
    COMMAND ${BREW_EXECUTABLE} --prefix catch2
    OUTPUT_VARIABLE HOMEBREW_CATCH2_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif()

if(HOMEBREW_LLVM_PREFIX)
  list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_LLVM_PREFIX}")
endif()
if(HOMEBREW_CATCH2_PREFIX)
  list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_CATCH2_PREFIX}")
endif()

find_package(LLVM CONFIG REQUIRED)
message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION} from ${LLVM_DIR}")

find_package(Catch2 3 REQUIRED)
find_package(Python3 COMPONENTS Interpreter)

find_library(LLVM_DYLIB
  NAMES LLVM-23 LLVM
  HINTS ${LLVM_LIBRARY_DIRS})
if(NOT LLVM_DYLIB)
  llvm_map_components_to_libnames(LLVM_DYLIB core support target)
endif()
message(STATUS "Linking LLVM library: ${LLVM_DYLIB}")

set(C4_CORE_SOURCES
  src/Support.cpp
  src/Token.cpp
  src/Lexer.cpp
  src/SymbolInternalizer.cpp
  src/AST.cpp
  src/Type.cpp
  src/SemanticModel.cpp
  src/Parser.cpp
  src/Sema.cpp
  src/Printer.cpp
  src/CodeGen.cpp
  src/Optimizer.cpp
  src/SCCP.cpp)

add_library(c4core STATIC ${C4_CORE_SOURCES})
target_include_directories(c4core PUBLIC src)
target_include_directories(c4core SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
target_compile_definitions(c4core PUBLIC
  __STDC_CONSTANT_MACROS __STDC_FORMAT_MACROS __STDC_LIMIT_MACROS)
target_compile_options(c4core PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(c4core PUBLIC ${LLVM_DYLIB})

add_executable(c4 src/main.cpp src/Compiler.cpp)
target_link_libraries(c4 PRIVATE c4core)
target_compile_options(c4 PRIVATE -Wall -Wextra)
set_target_properties(c4 PROPERTIES
  BUILD_RPATH "${LLVM_LIBRARY_DIRS}"
  INSTALL_RPATH "${LLVM_LIBRARY_DIRS}")

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

add_executable(c4_tests
  LexerTests.cpp
  ParserTests.cpp
  SemaTests.cpp
  PrinterTests.cpp
  CodeGenTests.cpp
  SCCPTests.cpp)
target_link_libraries(c4_tests PRIVATE c4core Catch2::Catch2WithMain)
target_include_directories(c4_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(c4_tests PRIVATE
  C4_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_options(c4_tests PRIVATE -Wall -Wextra)

include(Catch)
catch_discover_tests(c4_tests)

find_program(CLANG_EXECUTABLE NAMES clang)
if(Python3_Interpreter_FOUND AND CLANG_EXECUTABLE)
  add_test(NAME e2e
    COMMAND Python3::Interpreter
      ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_e2e.py
      --c4 $<TARGET_FILE:c4>
      --clang ${CLANG_EXECUTABLE}
      --programs ${CMAKE_CURRENT_SOURCE_DIR}/e2e/programs
      --rejects ${CMAKE_CURRENT_SOURCE_DIR}/e2e/rejects)
  set_tests_properties(e2e PROPERTIES TIMEOUT 300)
endif()

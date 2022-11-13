find_package(Catch2 3 REQUIRED)
add_executable(testlib Lexer.cpp)
target_include_directories(testlib PRIVATE ../src)
target_compile_features(testlib PRIVATE cxx_std_17)
target_link_libraries(testlib PRIVATE compiler Catch2::Catch2WithMain)

add_test(NAME testlibtest COMMAND testlib)
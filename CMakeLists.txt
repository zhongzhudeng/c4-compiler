cmake_minimum_required(VERSION 3.0.0)
project(
  c4
  VERSION 0.1.0
  LANGUAGES CXX)

set(CMAKE_COMPILE_FLAGS "-W -Wall -pedantic")

include(CTest)
enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

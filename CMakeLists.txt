cmake_minimum_required(VERSION 3.20)
project(TAVIQ VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(taviq
    src/main.cpp
    compiler/lexer.cpp
    compiler/parser.cpp
    runtime/interpreter.cpp
)
target_include_directories(taviq PRIVATE include)
if(MSVC)
    target_compile_options(taviq PRIVATE /W4 /permissive-)
else()
    target_compile_options(taviq PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(taviq_tests tests/unit/prototype_tests.cpp
    compiler/lexer.cpp
)
target_include_directories(taviq_tests PRIVATE include)
add_test(NAME taviq_prototype_tests COMMAND taviq_tests)

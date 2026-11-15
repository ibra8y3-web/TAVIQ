cmake_minimum_required(VERSION 3.20)
project(TAVIQ VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)

add_library(taviq_core
    compiler/lexer.cpp
    compiler/parser.cpp
    compiler/compiler.cpp
    compiler/bytecode.cpp
    compiler/native.cpp
    runtime/vm.cpp
    runtime/interpreter.cpp
    compute/parallel.cpp
    compute/gpu_provider.cpp
)
target_include_directories(taviq_core PUBLIC include)
target_link_libraries(taviq_core PUBLIC Threads::Threads \${CMAKE_DL_LIBS})
if(MSVC)
    target_compile_options(taviq_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(taviq_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(taviq src/main.cpp)
target_link_libraries(taviq PRIVATE taviq_core)

enable_testing()
add_executable(taviq_tests tests/unit/prototype_tests.cpp)
target_link_libraries(taviq_tests PRIVATE taviq_core)
add_test(NAME taviq_unit_tests COMMAND taviq_tests)

add_library(taviq_test_compute_provider MODULE tests/fixtures/compute_provider.cpp)
target_include_directories(taviq_test_compute_provider PRIVATE include)
target_compile_definitions(taviq_test_compute_provider PRIVATE TAVIQ_COMPUTE_PROVIDER_BUILD)
set_target_properties(taviq_test_compute_provider PROPERTIES PREFIX "")
add_executable(taviq_gpu_provider_tests tests/unit/gpu_provider_tests.cpp)
target_link_libraries(taviq_gpu_provider_tests PRIVATE taviq_core)
target_compile_definitions(taviq_gpu_provider_tests PRIVATE
    TAVIQ_TEST_COMPUTE_PROVIDER_PATH="$<TARGET_FILE:taviq_test_compute_provider>")
add_dependencies(taviq_gpu_provider_tests taviq_test_compute_provider)
add_test(NAME taviq_gpu_provider_tests COMMAND taviq_gpu_provider_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME taviq_cli_integration
        COMMAND \${Python3_EXECUTABLE}
                \${CMAKE_CURRENT_SOURCE_DIR}/tests/python/test_cli.py
                $<TARGET_FILE:taviq>
                "\${CMAKE_C_COMPILER}")
    add_test(NAME taviq_python_tool_tests
        COMMAND \${Python3_EXECUTABLE} -m unittest discover -s \${CMAKE_CURRENT_SOURCE_DIR}/tests/python -v)
endif()

install(TARGETS taviq RUNTIME DESTINATION bin)
install(TARGETS taviq_core ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(DIRECTORY include/taviq DESTINATION include)

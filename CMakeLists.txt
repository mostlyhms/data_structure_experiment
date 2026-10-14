cmake_minimum_required(VERSION 3.20)
project(StudentManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -pedantic)

# Shared code: validation, score handling, file persistence and both store implementations.
add_library(student_core STATIC
        error.h
        student.h
        score.h score.cpp
        validate.h validate.cpp
        storage.h storage.cpp
        store.h
        list_store.h list_store.cpp
        bst_store.h bst_store.cpp)
target_include_directories(student_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Interactive menu, kept separate from the core so tests can drive it with string streams.
add_library(student_cli STATIC cli.h cli.cpp)
target_link_libraries(student_cli PUBLIC student_core)

# Command-line program. Optional argument: data file path (default: students.txt in the working directory).
add_executable(student_manager main.cpp)
target_link_libraries(student_manager PRIVATE student_cli)

# Phase tests. test_phase2 also accepts "--bench" to time 100000 records.
add_executable(test_phase1 test_phase1.cpp)
target_link_libraries(test_phase1 PRIVATE student_core)

add_executable(test_phase2 test_phase2.cpp)
target_link_libraries(test_phase2 PRIVATE student_core)

add_executable(test_phase3 test_phase3.cpp)
target_link_libraries(test_phase3 PRIVATE student_cli)

enable_testing()
add_test(NAME phase1 COMMAND test_phase1)
add_test(NAME phase2 COMMAND test_phase2)
add_test(NAME phase3 COMMAND test_phase3)

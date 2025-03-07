cmake_minimum_required(VERSION 3.25)
project(carptest LANGUAGES CXX C)

# Built using Catch2 v2.13.10-1
find_package(Catch2 2.13 REQUIRED)

add_executable(carptest
    ${CMAKE_CURRENT_SOURCE_DIR}/carp_test_all.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/carp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/carp_argument_vector.c)

target_compile_definitions(carptest PRIVATE CARP_UNIT_TEST)
target_include_directories(carptest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_link_libraries(carptest Catch2::Catch2WithMain)

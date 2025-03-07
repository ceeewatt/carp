cmake_minimum_required(VERSION 3.25)
project(carp LANGUAGES C)

include(CTest)

if (NOT DEFINED CARP_JSON_FILE)
    message(FATAL_ERROR "[carp] CARP_JSON_FILE must be set to a valid file")
endif()

if (NOT EXISTS ${CARP_JSON_FILE})
    message(FATAL_ERROR "[carp] cannot find provided json file ${CARP_JSON_FILE}")
else()
    # Find the absolute path to input json file.
    # If the provided path is relative, the absolute path will be
    #  evaluated relative to the top-level cmake directory.
    file(REAL_PATH ${CARP_JSON_FILE} CARP_JSON_FILE
        BASE_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if (NOT DEFINED CARP_IMPLEMENTATION)
    set(CARP_IMPLEMENTATION "search")
endif()

if (${CARP_IMPLEMENTATION} STREQUAL "hash")
    set(PYTHON_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/carp_hash.c)
    set(PYTHON_ARGS hash ${CMAKE_CURRENT_BINARY_DIR} ${CARP_JSON_FILE})
    set(CARP_IMPLEMENTATION CARP_IMPLEMENTATION_HASH)
else()
    set(PYTHON_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/carp_search.c)
    set(PYTHON_ARGS search ${CMAKE_CURRENT_BINARY_DIR} ${CARP_JSON_FILE})
    set(CARP_IMPLEMENTATION CARP_IMPLEMENTATION_SEARCH)
endif()

set(CARP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CARP_PY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/py)

# Generate 'PYTHON_OUTPUT' which is used to build the static library.
# See: https://cmake.org/cmake/help/latest/command/add_custom_command.html#examples-generating-files
add_custom_command(
  OUTPUT ${PYTHON_OUTPUT}
  COMMAND python3 ${CARP_PY_DIR}/carp.py ${PYTHON_ARGS}
  DEPENDS ${CARP_JSON_FILE} ${CARP_PY_DIR}/carp.py
  VERBATIM)

add_library(carp STATIC
    ${PYTHON_OUTPUT}
    ${CARP_SRC_DIR}/carp.c
    ${CARP_SRC_DIR}/carp.h
    ${CARP_SRC_DIR}/carp_backend.c
    ${CARP_SRC_DIR}/carp_backend.h
    ${CARP_SRC_DIR}/carp_argument_vector.c
    ${CARP_SRC_DIR}/carp_argument_vector.h)

target_include_directories(carp PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CARP_SRC_DIR})
target_compile_definitions(carp PRIVATE ${CARP_IMPLEMENTATION})

if (${CARP_ENABLE_TESTING})
    add_subdirectory(test)
    add_test(NAME carp_test_all COMMAND carptest)
    enable_testing()
endif()

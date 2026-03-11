cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(agent-probe VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
cmake_policy(SET CMP0169 OLD)

# ─── tree-sitter (core C library) ───────────────────────────────
FetchContent_Declare(
    tree_sitter
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
    GIT_TAG        v0.24.6
    GIT_SHALLOW    TRUE
)

FetchContent_GetProperties(tree_sitter)
if(NOT tree_sitter_POPULATED)
    FetchContent_Populate(tree_sitter)
endif()

add_library(tree_sitter_lib STATIC
    ${tree_sitter_SOURCE_DIR}/lib/src/lib.c
)

target_include_directories(tree_sitter_lib
    PUBLIC  ${tree_sitter_SOURCE_DIR}/lib/include
    PRIVATE ${tree_sitter_SOURCE_DIR}/lib/src
)

set_target_properties(tree_sitter_lib PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# ─── tree-sitter-python (grammar) ──────────────────────────────
FetchContent_Declare(
    tree_sitter_python
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-python.git
    GIT_TAG        v0.23.6
    GIT_SHALLOW    TRUE
)

FetchContent_GetProperties(tree_sitter_python)
if(NOT tree_sitter_python_POPULATED)
    FetchContent_Populate(tree_sitter_python)
endif()

add_library(tree_sitter_python_lib STATIC
    ${tree_sitter_python_SOURCE_DIR}/src/parser.c
    ${tree_sitter_python_SOURCE_DIR}/src/scanner.c
)

target_include_directories(tree_sitter_python_lib
    PRIVATE
        ${tree_sitter_python_SOURCE_DIR}/src
        ${tree_sitter_SOURCE_DIR}/lib/include
)

target_link_libraries(tree_sitter_python_lib PRIVATE tree_sitter_lib)

set_target_properties(tree_sitter_python_lib PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# ─── CLI11 (header-only, manual setup) ─────────────────────────
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.3.2
    GIT_SHALLOW    TRUE
)

FetchContent_GetProperties(cli11)
if(NOT cli11_POPULATED)
    FetchContent_Populate(cli11)
endif()

add_library(cli11_headers INTERFACE)
target_include_directories(cli11_headers INTERFACE ${cli11_SOURCE_DIR}/include)

# ─── nlohmann/json ─────────────────────────────────────────────
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
    FetchContent_Populate(json)
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR})
endif()

# ─── Main executable ───────────────────────────────────────────
add_executable(agent-probe
    src/main.cpp
    src/parser/ts_parser.cpp
    src/graph/graph.cpp
    src/graph/builder.cpp
    src/graph/algorithms.cpp
)

target_include_directories(agent-probe PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(agent-probe
    PRIVATE
        tree_sitter_lib
        tree_sitter_python_lib
        cli11_headers
        nlohmann_json::nlohmann_json
)

# ─── GoogleTest ────────────────────────────────────────────────
enable_testing()

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

add_executable(agent-probe-tests
    tests/cpp/test_placeholder.cpp
    tests/cpp/test_parser.cpp
    src/parser/ts_parser.cpp
    src/graph/graph.cpp
    src/graph/builder.cpp
    src/graph/algorithms.cpp
)

target_include_directories(agent-probe-tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(agent-probe-tests
    PRIVATE
        GTest::gtest_main
        tree_sitter_lib
        tree_sitter_python_lib
        nlohmann_json::nlohmann_json
)

include(GoogleTest)
gtest_discover_tests(agent-probe-tests)

cmake_minimum_required(VERSION 3.20)
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

# ─── Main executable ───────────────────────────────────────────
add_executable(agent-probe
    src/main.cpp
)

target_include_directories(agent-probe PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(agent-probe
    PRIVATE
        tree_sitter_lib
        tree_sitter_python_lib
)

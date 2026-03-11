cmake_minimum_required(VERSION 3.20)
project(agent-probe VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Main executable
add_executable(agent-probe
    src/main.cpp
)

target_include_directories(agent-probe PRIVATE ${CMAKE_SOURCE_DIR}/src)

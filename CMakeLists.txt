cmake_minimum_required(VERSION 3.16)
project(minicc C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM headers: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM cmake dir: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Source files
add_executable(minicc
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
)

# Link LLVM components
llvm_map_components_to_libnames(LLVM_LIBS
    Core
    Support
    BitWriter
    Analysis
    IRReader
)

target_link_libraries(minicc PRIVATE ${LLVM_LIBS})

# Warnings
if (MSVC)
    target_compile_options(minicc PRIVATE /W3)
else()
    target_compile_options(minicc PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Optional: Release build strips debug
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(minicc PRIVATE -O2)
endif()

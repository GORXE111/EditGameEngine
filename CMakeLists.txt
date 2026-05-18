cmake_minimum_required(VERSION 3.24)
project(FarmCode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# Put all binaries in one place so the game can find assets/DLLs.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
  add_compile_options(/utf-8 /W4 /permissive-)
endif()

enable_testing()

include(cmake/dependencies.cmake)

# Engine-independent libraries (must stay headless-testable).
add_subdirectory(src/lang)
add_subdirectory(src/vm)
add_subdirectory(src/sim)
add_subdirectory(src/blueprint)

# Game executable (depends on SDL2 + ImGui + imnodes).
add_subdirectory(src/game)

# Headless golden tests.
add_subdirectory(tests)

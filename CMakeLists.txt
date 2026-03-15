cmake_minimum_required(VERSION 3.16)
project(cube_solver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cube_solver
  src/main.cpp
)

target_compile_definitions(cube_solver PRIVATE
  PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_options(cube_solver PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

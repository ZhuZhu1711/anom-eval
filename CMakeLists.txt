cmake_minimum_required(VERSION 3.16)

# This environment's /usr/bin/c++ is clang without libstdc++ on the link path.
if(NOT CMAKE_CXX_COMPILER AND EXISTS "/usr/bin/g++")
  set(CMAKE_CXX_COMPILER "/usr/bin/g++")
endif()

project(dd_ab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -O2)

add_executable(dd_ab
  src/algorithm_common.cpp
  src/algorithm_control.cpp
  src/algorithm_experimental.cpp
  src/oracle.cpp
  src/casefile.cpp
  src/generator.cpp
  src/compare.cpp
  src/main.cpp
)
target_include_directories(dd_ab PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Frozen coverage grid, regenerated on demand via: dd_ab --write-coverage
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/testdata/generated_coverage.txt
  COMMAND $<TARGET_FILE:dd_ab> --write-coverage ${CMAKE_SOURCE_DIR}/testdata/generated_coverage.txt
  DEPENDS dd_ab
  COMMENT "Generating frozen coverage case file"
)
add_custom_target(coverage_file
  DEPENDS ${CMAKE_SOURCE_DIR}/testdata/generated_coverage.txt)

enable_testing()
add_test(NAME ab_quick
  COMMAND dd_ab --data-dir ${CMAKE_SOURCE_DIR}/testdata --random 400 --width 64 --height 48 --defects 12 --seed 1
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

cmake_minimum_required(VERSION 3.28)

project(SimpleFastFloatBenchmark VERSION 0.1.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0169 OLD) # Prevent warning when using FetchContent_Populate
endif()

include(CheckCXXCompilerFlag)
set(SIMPLE_FAST_FLOAT_BENCHMARK_MARCH "" CACHE STRING
    "Set -march value for benchmarks (e.g. native, x86-64-v4, etc)")

if(NOT SIMPLE_FAST_FLOAT_BENCHMARK_MARCH)
  set(_SFF_MARCH_FLAG "-march=native")
else()
  set(_SFF_MARCH_FLAG "-march=${SIMPLE_FAST_FLOAT_BENCHMARK_MARCH}")
endif()

CHECK_CXX_COMPILER_FLAG(${_SFF_MARCH_FLAG} _SFF_MARCH_SUPPORTED)
if(_SFF_MARCH_SUPPORTED)
  add_compile_options(${_SFF_MARCH_FLAG})
  message(STATUS "Compiling with ${_SFF_MARCH_FLAG}")
else()
  message(STATUS "${_SFF_MARCH_FLAG} not supported, proceeding without -march flag.")
endif()

if (NOT CMAKE_BUILD_TYPE AND NOT SANITIZE)
  message(STATUS "No build type selected, default to Release")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
if(SANITIZE)
  add_compile_options(-fsanitize=address  -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_compile_definitions(ASAN_OPTIONS=detect_leaks=1)
  link_libraries(-fsanitize=address  -fno-omit-frame-pointer -fno-sanitize-recover=all)
endif()
include(cmake/CPM.cmake)

add_subdirectory(dependencies)
add_subdirectory(benchmarks)

cmake_minimum_required(VERSION 3.15)

project(SimpleFastFloatBenchmark VERSION 0.1.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0169 OLD) # Prevent warning when using FetchContent_Populate
endif()

include(CheckCXXCompilerFlag)
unset(FASTPFOR_COMPILER_SUPPORTS_MARCH_NATIVE CACHE)
CHECK_CXX_COMPILER_FLAG(-march=native FASTPFOR_COMPILER_SUPPORTS_MARCH_NATIVE)
if(FASTPFOR_COMPILER_SUPPORTS_MARCH_NATIVE)
    add_compile_options(-march=native)
else()
     message(STATUS "native target not supported")
endif()

if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type selected, default to Release")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
include(cmake/CPM.cmake)

add_subdirectory(dependencies)
add_subdirectory(benchmarks)
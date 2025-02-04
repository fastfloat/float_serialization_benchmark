cmake_minimum_required(VERSION 3.15)

project(SimpleFastFloatBenchmark VERSION 0.1.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0169 OLD) # Prevent warning when using FetchContent_Populate

if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type selected, default to Release")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
include(cmake/CPM.cmake)
add_subdirectory(dependencies)

add_subdirectory(benchmarks)

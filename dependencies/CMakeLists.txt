if(NOT WIN32)
    add_subdirectory(netlib)
endif()
add_subdirectory(grisu2)

include(FetchContent)
include(ExternalProject)

if (NOT CYGWIN)
    set(ABSL_PROPAGATE_CXX_STD ON)
    set(ABSL_ENABLE_INSTALL ON)
    set(ABSL_RUN_TEST OFF CACHE INTERNAL "")
    set(ABSL_USE_GOOGLETEST_HEAD OFF CACHE INTERNAL "")

    FetchContent_Declare(abseil
         GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
         GIT_TAG "d7aaad8")
    FetchContent_GetProperties(abseil)
    if(NOT abseil_POPULATED)
        set(BUILD_TESTING OFF)
        FetchContent_MakeAvailable(abseil)
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${abseil_SOURCE_DIR}/absl/copts)
        include(${abseil_SOURCE_DIR}/absl/copts/AbseilConfigureCopts.cmake)
    endif()
endif()

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Disable deprecation warnings" FORCE)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

FetchContent_Declare(
        doubleconversion
        GIT_REPOSITORY https://github.com/google/double-conversion.git
        GIT_TAG "v3.1.5"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doubleconversion)

FetchContent_Declare(
        teju_jagua
        GIT_REPOSITORY https://github.com/jaja360/teju_jagua.git
        GIT_TAG main
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(teju_jagua)
get_property(teju_has_uint128 DIRECTORY ${teju_jagua_SOURCE_DIR} PROPERTY teju_has_uint128)
# When teju_has_float128 is set, then we will need teju_boost_multiprecision
get_property(teju_has_float128 DIRECTORY ${teju_jagua_SOURCE_DIR} PROPERTY teju_has_float128)
if(teju_has_float128)
        message(STATUS "support for 128-bit floats detected by teju")
else()
        message(STATUS "support for 128-bit floats NOT detected by teju")
endif()

#CPMAddPackage("gh:ulfjack/ryu@2.0")
FetchContent_Declare(
        ryu
        GIT_REPOSITORY https://github.com/ulfjack/ryu
        GIT_TAG master
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ryu)

FetchContent_Declare(
        grisu-exact
        GIT_REPOSITORY https://github.com/jk-jeon/Grisu-Exact
        GIT_TAG master
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(grisu-exact)
set(grisu-exact_SOURCE_DIR ${grisu-exact_SOURCE_DIR} PARENT_SCOPE)

FetchContent_Declare(
        dragonbox
        GIT_REPOSITORY https://github.com/jk-jeon/dragonbox
        GIT_TAG tags/1.1.3
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(dragonbox)

FetchContent_Declare(
        drachennest # for schubfach and dragon4
        GIT_REPOSITORY https://github.com/abolz/Drachennest.git
        GIT_TAG master
        GIT_SHALLOW TRUE
)
FetchContent_GetProperties(drachennest)
if(NOT drachennest_POPULATED)
  FetchContent_Populate(drachennest)  # Downloads code to ${drachennest_SOURCE_DIR}
endif()
set(drachennest_SOURCE_DIR/src ${drachennest_SOURCE_DIR}/src PARENT_SCOPE)
add_library(dragon_schubfach_lib STATIC
    ${drachennest_SOURCE_DIR}/src/dragon4.cc
    ${drachennest_SOURCE_DIR}/src/schubfach_32.cc
    ${drachennest_SOURCE_DIR}/src/schubfach_64.cc
)
target_include_directories(dragon_schubfach_lib PUBLIC
    ${drachennest_SOURCE_DIR}/src
)

CPMAddPackage("gh:fmtlib/fmt#11.0.2")
CPMAddPackage(
  GITHUB_REPOSITORY jarro2783/cxxopts
  VERSION 3.2.0
  OPTIONS "CXXOPTS_BUILD_EXAMPLES NO" "CXXOPTS_BUILD_TESTS NO" "CXXOPTS_ENABLE_INSTALL YES"
)

if(NOT WIN32)
  CPMAddPackage(
    NAME errol
    GITHUB_REPOSITORY marcandrysco/Errol
    GIT_TAG 5364de4
    DOWNLOAD_ONLY ON
  )

  add_library(errol STATIC "${errol_SOURCE_DIR}/lib/errol.c")
  target_include_directories(errol SYSTEM PUBLIC "${errol_SOURCE_DIR}/lib")
endif()

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
#cmake_policy(SET CMP0077 NEW)
FetchContent_Declare(doubleconversion
     GIT_REPOSITORY https://github.com/google/double-conversion.git
     GIT_TAG "v3.1.5")
FetchContent_GetProperties(doubleconversion)
FetchContent_MakeAvailable(doubleconversion)

#CPMAddPackage("gh:ulfjack/ryu@2.0")
include(FetchContent)
FetchContent_Declare(
        ryu
        GIT_REPOSITORY https://github.com/ulfjack/ryu
        GIT_TAG master
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(ryu)

FetchContent_Declare(
        dragonbox
        GIT_REPOSITORY https://github.com/jk-jeon/dragonbox
        GIT_TAG tags/1.1.3
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(dragonbox)

CPMAddPackage("gh:fmtlib/fmt#11.0.2")
CPMAddPackage(
  GITHUB_REPOSITORY jarro2783/cxxopts
  VERSION 3.2.0
  OPTIONS "CXXOPTS_BUILD_EXAMPLES NO" "CXXOPTS_BUILD_TESTS NO" "CXXOPTS_ENABLE_INSTALL YES"
)

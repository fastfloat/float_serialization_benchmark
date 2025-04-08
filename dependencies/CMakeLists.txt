if(NOT WIN32)
    add_subdirectory(netlib)
endif()
add_subdirectory(yy_double)
#add_subdirectory(grisu2)
find_library(MATH_LIBRARY m)
include(FetchContent)
include(ExternalProject)

if (NOT CYGWIN)
    set(ABSL_PROPAGATE_CXX_STD ON)
    set(ABSL_ENABLE_INSTALL ON)
    set(ABSL_RUN_TEST OFF CACHE INTERNAL "")
    set(ABSL_USE_GOOGLETEST_HEAD OFF CACHE INTERNAL "")

    FetchContent_Declare(abseil
         GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
         GIT_TAG "d9e4955") # Abseil LTS branch, January 2025, Patch 1
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
        GIT_TAG 101e1ba89dc41ceb75090831da97c43a76cd2906
        # GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doubleconversion)

FetchContent_Declare(
        teju_jagua
        GIT_REPOSITORY https://github.com/jaja360/teju_jagua.git
        GIT_TAG 990e89aa886a19d29c5b2e0f93d150f74c0b0d4e
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
        GIT_TAG 1264a946ba66eab320e927bfd2362e0c8580c42f
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ryu)
set(ryu_SOURCE_DIR ${ryu_SOURCE_DIR} PARENT_SCOPE)

FetchContent_Declare(
        grisu-exact
        GIT_REPOSITORY https://github.com/jk-jeon/Grisu-Exact
        GIT_TAG 4279e63faa301c03112f56f9ee4ed8bdff947fee
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
        dragon4
        GIT_REPOSITORY https://github.com/lemire/Dragon4.git
        GIT_TAG 0ce72aa
        GIT_SHALLOW TRUE
)
FetchContent_GetProperties(dragon4)
if(NOT dragon4)
  FetchContent_Populate(dragon4)  # Downloads code to ${dragon4_SOURCE_DIR}
endif()
set(dragon4_SOURCE_DIR ${dragon4_SOURCE_DIR} PARENT_SCOPE)
add_library(dragon4_lib STATIC
    ${dragon4_SOURCE_DIR}/Dragon4.cpp
    ${dragon4_SOURCE_DIR}/DragonMath.cpp
    ${dragon4_SOURCE_DIR}/PrintFloat.cpp
)
if(MATH_LIBRARY)
    target_link_libraries(dragon4_lib PUBLIC ${MATH_LIBRARY})
endif()
target_include_directories(dragon4_lib PUBLIC
    ${dragon4_SOURCE_DIR}
)

# Swift code may not build under Windows without extra care.
if(NOT WIN32)
        FetchContent_Declare(
                swift
                GIT_REPOSITORY https://github.com/swiftlang/swift.git
                GIT_TAG 6a862d2eb7128ff1f317b07e8ad1a6da939775f3
                GIT_SHALLOW TRUE
        )
        FetchContent_GetProperties(swift)
        if(NOT swift)
                FetchContent_Populate(swift)  # Downloads code to ${dragon4_SOURCE_DIR}
        endif()
        set(swift_SOURCE_DIR ${swift_SOURCE_DIR} PARENT_SCOPE)
        add_library(swift_lib STATIC
                ${swift_SOURCE_DIR}/stdlib/public/runtime/SwiftDtoa.cpp
        )
        if(MATH_LIBRARY)
                target_link_libraries(swift_lib PUBLIC ${MATH_LIBRARY})
        endif()
        target_include_directories(swift_lib PUBLIC
           ${swift_SOURCE_DIR}/include
        )
endif(NOT WIN32)

FetchContent_Declare(
        drachennest # for schubfach
        GIT_REPOSITORY https://github.com/abolz/Drachennest.git
        GIT_TAG e6714a39ad331b4489d0b6aaf3968635bff4eb5e
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
    ${drachennest_SOURCE_DIR}/src/grisu3.cc
    ${drachennest_SOURCE_DIR}/src/grisu2.cc
)
target_include_directories(dragon_schubfach_lib PUBLIC
    ${drachennest_SOURCE_DIR}/src
)

CPMAddPackage("gh:fmtlib/fmt#11.1.4")
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

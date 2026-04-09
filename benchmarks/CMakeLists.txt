
include_directories(../include)
add_executable(benchmark
    benchmark.cpp
)
add_library(benchmark_deps INTERFACE)
add_library(ieeeToString ieeeToString.cpp)
target_link_libraries(benchmark_deps INTERFACE ieeeToString)
include(CheckSourceCompiles)
check_source_compiles(CXX "
#include <charconv>
int main(void) {
    float valuef = 0;
    double valued = 0;
    const char* ptr;
    std::from_chars_result result1 = std::from_chars(ptr, ptr, valuef, std::chars_format::general);
    std::from_chars_result result2 = std::from_chars(ptr, ptr, valued, std::chars_format::general);
    return 0;
}
" FROM_CHARS_OK)
target_compile_definitions(benchmark_deps INTERFACE FROM_CHARS_SUPPORTED=$<IF:$<BOOL:${FROM_CHARS_OK}>,1,0>)
check_source_compiles(CXX "
#include <charconv>
int main(void) {
    float valuef = 0;
    double valued = 0;
    char* ptr;
    std::to_chars_result result1 = std::to_chars(ptr, ptr, valuef);
    std::to_chars_result result2 = std::to_chars(ptr, ptr, valued);
    return 0;
}
" TO_CHARS_OK)
target_compile_definitions(benchmark_deps INTERFACE TO_CHARS_SUPPORTED=$<IF:$<BOOL:${TO_CHARS_OK}>,1,0>)
if(TO_CHARS_OK)
  message(STATUS "std::to_chars with floats is supported")
else(TO_CHARS_OK)
  message(STATUS "std::to_chars with floats is NOT supported")
endif(TO_CHARS_OK)
if(FROM_CHARS_OK)
  message(STATUS "std::from_chars with floats is supported")
else(FROM_CHARS_OK)
  message(STATUS "std::from_chars with floats is NOT supported")
endif(FROM_CHARS_OK)
CPMAddPackage(
  NAME fast_float
  GITHUB_REPOSITORY "fastfloat/fast_float"
  GIT_TAG v8.0.2)
target_link_libraries(benchmark_deps INTERFACE fast_float)
if (NOT WIN32)
    target_link_libraries(benchmark_deps INTERFACE netlib)
    target_compile_definitions(benchmark_deps INTERFACE NETLIB_SUPPORTED=1)
else()
    target_compile_definitions(benchmark_deps INTERFACE NETLIB_SUPPORTED=0)
endif()

if (NOT CYGWIN)
    target_link_libraries(benchmark_deps INTERFACE absl::str_format)
    target_compile_definitions(benchmark_deps INTERFACE ABSEIL_SUPPORTED=1)
else()
    target_compile_definitions(benchmark_deps INTERFACE ABSEIL_SUPPORTED=0)
endif()

if(TARGET errol)
    target_link_libraries(benchmark_deps INTERFACE errol)
    target_compile_definitions(benchmark_deps INTERFACE ERROL_SUPPORTED=1)
else()
    target_compile_definitions(benchmark_deps INTERFACE ERROL_SUPPORTED=0)
endif()

if(TARGET xjb AND NOT MSVC)
    target_link_libraries(benchmark_deps INTERFACE xjb)
    target_compile_definitions(benchmark_deps INTERFACE XJB_SUPPORTED=1)
else()
    target_compile_definitions(benchmark_deps INTERFACE XJB_SUPPORTED=0)
endif()

target_link_libraries(benchmark_deps INTERFACE fmt)
target_link_libraries(benchmark_deps INTERFACE cxxopts)

target_link_libraries(benchmark_deps INTERFACE yy_double)
target_link_libraries(benchmark_deps INTERFACE double-conversion)
target_link_libraries(benchmark_deps INTERFACE ryu::ryu)
target_link_libraries(benchmark_deps INTERFACE teju)
if(teju_has_float128)
    target_link_libraries(benchmark_deps INTERFACE teju_boost_multiprecision)
endif()
target_link_libraries(benchmark_deps INTERFACE dragonbox::dragonbox_to_chars)
target_link_libraries(benchmark_deps INTERFACE dragon_schubfach_lib)
target_link_libraries(benchmark_deps INTERFACE dragon4_lib)
if(TARGET swift_lib)
    target_link_libraries(benchmark_deps INTERFACE swift_lib)
    target_compile_definitions(benchmark_deps INTERFACE SWIFT_LIB_SUPPORTED=1)
else(TARGET swift_lib)
    target_compile_definitions(benchmark_deps INTERFACE SWIFT_LIB_SUPPORTED=0)
endif(TARGET swift_lib)
target_include_directories(benchmark_deps INTERFACE ${grisu-exact_SOURCE_DIR})

add_library(zmij_lib STATIC ${zmij_SOURCE_DIR}/zmij.cc)
target_include_directories(zmij_lib PUBLIC ${zmij_SOURCE_DIR})
target_link_libraries(benchmark_deps INTERFACE zmij_lib)

target_link_libraries(benchmark PUBLIC benchmark_deps)

if(TO_CHARS_OK AND FROM_CHARS_OK)
    add_executable(exhaustivefloat32
        exhaustivefloat32.cpp
    )

    target_link_libraries(exhaustivefloat32 PUBLIC benchmark_deps)
    add_executable(thoroughfloat64
        thoroughfloat64.cpp
    )
    set(THOROUGH_DATA_FILE "${CMAKE_SOURCE_DIR}/data/thoroughnumbers.txt")
    target_compile_definitions(thoroughfloat64 PRIVATE THOROUGH_DATA_FILE="${THOROUGH_DATA_FILE}")

    target_link_libraries(thoroughfloat64 PUBLIC benchmark_deps)
endif(TO_CHARS_OK AND FROM_CHARS_OK)

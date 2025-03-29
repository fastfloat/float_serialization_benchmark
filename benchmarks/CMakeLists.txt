add_executable(benchmark
    benchmark.cpp
    ieeeToString.cpp
)

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
target_compile_definitions(benchmark PUBLIC FROM_CHARS_SUPPORTED=$<IF:$<BOOL:${FROM_CHARS_OK}>,1,0>)
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
target_compile_definitions(benchmark PUBLIC TO_CHARS_SUPPORTED=$<IF:$<BOOL:${TO_CHARS_OK}>,1,0>)
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
target_link_libraries(benchmark PUBLIC fast_float)
if (NOT WIN32)
    target_link_libraries(benchmark PUBLIC netlib)
    target_compile_definitions(benchmark PUBLIC NETLIB_SUPPORTED=1)
else()
    target_compile_definitions(benchmark PUBLIC NETLIB_SUPPORTED=0)
endif()

if (NOT CYGWIN)
    target_link_libraries(benchmark PUBLIC absl::str_format)
    target_compile_definitions(benchmark PUBLIC ABSEIL_SUPPORTED=1)
else()
    target_compile_definitions(benchmark PUBLIC ABSEIL_SUPPORTED=0)
endif()

if(TARGET errol)
    target_link_libraries(benchmark PUBLIC errol)
    target_compile_definitions(benchmark PUBLIC ERROL_SUPPORTED=1)
else()
    target_compile_definitions(benchmark PUBLIC ERROL_SUPPORTED=0)
endif()

target_link_libraries(benchmark PUBLIC fmt)
target_link_libraries(benchmark PUBLIC cxxopts)

#target_link_libraries(benchmark PUBLIC grisu2)
target_link_libraries(benchmark PUBLIC double-conversion)
target_link_libraries(benchmark PUBLIC ryu::ryu)
target_link_libraries(benchmark PUBLIC teju)
if(teju_has_float128)
    target_link_libraries(benchmark PUBLIC teju_boost_multiprecision)
endif()
target_link_libraries(benchmark PUBLIC dragonbox::dragonbox_to_chars)
target_link_libraries(benchmark PUBLIC dragon_schubfach_lib)
target_link_libraries(benchmark PUBLIC dragon4_lib)
target_include_directories(benchmark PUBLIC ${grisu-exact_SOURCE_DIR})

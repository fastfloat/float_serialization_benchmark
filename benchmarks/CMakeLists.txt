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
" from_chars_ok)

if (from_chars_ok)
    target_compile_definitions(benchmark PUBLIC FROM_CHARS_SUPPORTED=1)
endif()

if (NOT WIN32)
    target_link_libraries(benchmark PUBLIC netlib)
    target_compile_definitions(benchmark PUBLIC NETLIB_SUPPORTED=1)
endif()

if (NOT CYGWIN)
    target_link_libraries(benchmark PUBLIC absl::strings)
endif()

target_link_libraries(benchmark PUBLIC fmt)
target_link_libraries(benchmark PUBLIC cxxopts)

target_link_libraries(benchmark PUBLIC grisu2)
target_link_libraries(benchmark PUBLIC double-conversion)
target_link_libraries(benchmark PUBLIC ryu::ryu)
target_link_libraries(benchmark PUBLIC teju)
if(teju_has_float128)
    target_link_libraries(benchmark PUBLIC teju_boost_multiprecision)
endif()
target_link_libraries(benchmark PUBLIC dragonbox::dragonbox_to_chars)
target_link_libraries(benchmark PUBLIC dragon_schubfach_lib)
if(TARGET errol)
    target_link_libraries(benchmark PUBLIC errol)
endif()
target_include_directories(benchmark PUBLIC ${grisu-exact_SOURCE_DIR})

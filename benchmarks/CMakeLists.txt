add_executable(benchmark
    benchmark.cpp
    decimalToString.cpp
)

include(CheckSourceCompiles)
check_source_compiles(CXX "
#include <charconv>
int main(void) {
    double value = 0;
    const char* ptr;
    std::from_chars_result result = std::from_chars(ptr, ptr, value, std::chars_format::general);
    return 0;
}
" from_chars_double)

if (from_chars_double)
    target_compile_definitions(benchmark PUBLIC FROM_CHARS_DOUBLE_SUPPORTED=1)
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
target_link_libraries(benchmark PUBLIC dragonbox::dragonbox_to_chars)

# Source: https://github.com/ibireme/yyjson/issues/200#issuecomment-2726783097
set(HEADER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/yy_double.h)
add_library(yy_double INTERFACE)
target_include_directories(yy_double INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

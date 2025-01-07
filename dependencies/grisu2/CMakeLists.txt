
set(HEADER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/grisu2.h)
add_library(grisu2 INTERFACE)
target_include_directories(grisu2 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
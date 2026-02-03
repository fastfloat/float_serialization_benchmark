
# Assuming the Makefile is in the same directory as this CMakeLists.txt
set(MAKEFILE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/netlib_build)

# Define the output file(s) that the make command should produce
set(MAKEFILE_OUTPUT ${BUILD_DIR}/gdtoa.a) 

# Ensure the build directory exists
if(NOT EXISTS ${BUILD_DIR})
    file(MAKE_DIRECTORY ${BUILD_DIR})
endif()

add_custom_command(
    OUTPUT ${BUILD_DIR}/gdtoa.h
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${MAKEFILE_DIR} ${BUILD_DIR}
    COMMENT "Copying entire netlib directory to build directory ${BUILD_DIR}, creating ${BUILD_DIR}/gdtoa.h"
)

add_custom_command(
    OUTPUT ${MAKEFILE_OUTPUT}
    COMMAND make CFLAGS=${CMAKE_C_FLAGS} -C ${BUILD_DIR}
    DEPENDS ${BUILD_DIR}/gdtoa.h
    COMMENT "Building netlib using Makefile"
    BYPRODUCTS ${MAKEFILE_OUTPUT}
    VERBATIM
)

add_custom_target(build_netlib ALL
    DEPENDS ${MAKEFILE_OUTPUT}
)

add_library(netlib INTERFACE)
add_dependencies(netlib build_netlib)
target_include_directories(netlib INTERFACE ${BUILD_DIR})
target_link_libraries(netlib INTERFACE ${MAKEFILE_OUTPUT})

cmake_minimum_required(VERSION 3.21)

set(LIBRARY_NAME utils_lib)

# Source files
set(SOURCE_FILES
    ColorUtils.cpp
    des.cpp
    StringUtils.cpp
)

message(STATUS "Utils sources: ${SOURCE_FILES}")

# Create library
add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})

# Include directories
target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src # Allows #include "utils/ColorUtils.hpp", etc.
    # Blend2D headers should be available via linking with the blend2d target.

    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} # Allows #include "ColorUtils.hpp", etc.
    ${CMAKE_SOURCE_DIR}/src/utils/
    )

# Link libraries
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
    blend2d # ColorUtils.cpp uses BLRgba32
)

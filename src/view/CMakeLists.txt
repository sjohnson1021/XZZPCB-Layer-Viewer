cmake_minimum_required(VERSION 3.21)

set(LIBRARY_NAME view_lib)

# Source files
set(SOURCE_FILES
    Camera.cpp
    Viewport.cpp
    GridSettings.cpp
    Grid.cpp
)

# Create library
add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})

# Include directories
target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src # Allows #include "view/Camera.hpp", etc.
    # Blend2D headers should be available via linking with the blend2d target.

    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} # Allows #include "Camera.hpp", etc.

    )

# Link libraries
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
    blend2d # Grid.cpp uses BLContext
    SDL3::SDL3 # Link against the SDL3 library target
)

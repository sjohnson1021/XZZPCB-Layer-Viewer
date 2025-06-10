cmake_minimum_required(VERSION 3.21)

set(LIBRARY_NAME render_lib)

# Source files
set(SOURCE_FILES
    PcbRenderer.cpp
    RenderContext.cpp
    RenderPipeline.cpp
)

# Create library
add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})

# Include directories
target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src # Allows #include "render/PcbRenderer.hpp", "core/Application.hpp" etc.
    # Blend2D headers should be available via linking with the blend2d target.
    PRIVATE
    # For includes within the render library itself, if any specific structure.
)

# Link libraries
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
    blend2d # Link against the Blend2D library target
    SDL3::SDL3 # Link against the SDL3 library target
    PRIVATE
    view_lib # Added dependency on view_lib
    utils_lib # For utility functions
)

# Ensure PcbRenderer.hpp and other headers can be found by users of this library
# The PUBLIC include directory above should handle this.

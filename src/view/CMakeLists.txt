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
)

# Link libraries
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
    blend2d # Grid.cpp uses BLContext
) 
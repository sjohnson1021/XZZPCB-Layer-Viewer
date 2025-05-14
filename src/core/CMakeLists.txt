set(LIBRARY_NAME core)

# Source files
set(SOURCE_FILES
    Application.cpp
    Config.cpp
    Events.cpp
    Renderer.cpp
    SDLRenderer.cpp
    BLRenderer.cpp
    ImGuiManager.cpp
    ImGuiRenderer.cpp
)

# ImGui source files - no need to include them here as we already built them in the root CMakeLists.txt
# Just reference the imgui target when linking

# Create library
add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})

# Include directories
target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
    ${CMAKE_SOURCE_DIR}/external/SDL/include
)

# Link libraries
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
    imgui
    SDL3::SDL3
) 
set(LIBRARY_NAME core_lib)

# Source files
set(SOURCE_FILES
    Application.cpp
    Config.cpp
    Events.cpp
    ControlSettings.cpp
    InputActions.cpp
    Renderer.cpp
    SDLRenderer.cpp
    BLRenderer.cpp
    ImGuiManager.cpp

    # PCB components
    ../pcb/PcbLoader.cpp
    ../pcb/Board.cpp
    # View components are now in view_lib
    # Utils components
    ../utils/des.cpp
    ../utils/StringUtils.cpp
)

# ImGui source files - no need to include them here as we already built them in the root CMakeLists.txt
# Just reference the imgui target when linking

# Create library
add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})

set_property(TARGET ${LIBRARY_NAME} PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

# Include directories
target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
    ${CMAKE_SOURCE_DIR}/external/SDL/include # This should now be handled by linking SDL3::SDL3 from vcpkg
    ${CMAKE_SOURCE_DIR}/external/ImGuiFileDialog
)

# Link libraries
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
    imgui
    SDL3::SDL3
    blend2d
    render_lib
    ui_lib
    view_lib
) 
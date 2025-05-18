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

    # UI components
    ../ui/MainMenuBar.cpp
    ../ui/windows/PCBViewerWindow.cpp
    ../ui/windows/SettingsWindow.cpp
    ../ui/windows/PcbDetailsWindow.cpp
    ../ui/interaction/InteractionManager.cpp
    ../ui/interaction/NavigationTool.cpp
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

# Include directories
target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
    ${CMAKE_SOURCE_DIR}/external/SDL/include
    ${CMAKE_SOURCE_DIR}/external/ImGuiFileDialog
)

# Link libraries
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
    view_lib   # Added dependency on view_lib
    imgui
    SDL3::SDL3
    blend2d
) 
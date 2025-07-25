cmake_minimum_required(VERSION 3.21)

set(LIBRARY_NAME core_lib)

# Source files
set(SOURCE_FILES
    # Application.cpp moved to main executable to break circular dependency
    Config.cpp
    Events.cpp
    ControlSettings.cpp
    InputActions.cpp
    Renderer.cpp
    SDLRenderer.cpp
    ImGuiManager.cpp

    # PCB components
    ../pcb/XZZPCBLoader.cpp
    # ../pcb/processing/OrientationProcessor.cpp
    ../pcb/Board.cpp
    ../pcb/BoardLoaderFactory.cpp
    ../pcb/elements/Arc.cpp
    ../pcb/elements/Component.cpp
    ../pcb/elements/Element.cpp
    ../pcb/elements/Pin.cpp
    ../pcb/elements/TextLabel.cpp
    ../pcb/elements/Trace.cpp
    ../pcb/elements/Via.cpp
    BoardDataManager.cpp
    # View components are now in view_lib
    # Utils components
    # ../utils/des.cpp
    # ../utils/StringUtils.cpp
    # ../utils/ColorUtils.cpp
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
    ${CMAKE_SOURCE_DIR}/external/SDL/include # This should now be handled by linking SDL3::SDL3 from vcpkg
    ${CMAKE_SOURCE_DIR}/external/ImGuiFileDialog
)

# Link libraries
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
    imgui
    SDL3::SDL3
    blend2d
    utils_lib
    PRIVATE
    render_lib
    view_lib
    # ui_lib # This creates a circular dependency - removed
)

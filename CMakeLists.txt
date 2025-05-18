cmake_minimum_required(VERSION 3.16)
project(PCBViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTS "Build tests" OFF)

# Find necessary packages
find_package(OpenGL REQUIRED)

# Organize output files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Dependencies (using Git Submodules in external/) ---

# SDL3
# Add SDL3 as a subdirectory. Assumes it has its own CMakeLists.txt that creates SDL3::SDL3 target.
# Ensure your submodule in external/SDL is SDL3.
add_subdirectory(external/SDL EXCLUDE_FROM_ALL) # EXCLUDE_FROM_ALL if you only want to link against it

# ImGui
# Add ImGui source files directly or create a library target for it.
# Assuming ImGui is in external/imgui and it's the docking branch.
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
# SDL3::SDL3 should be available from add_subdirectory(external/SDL)
# If SDL3 target is named differently (e.g. SDL3::main), adjust here.
# The imgui library itself will need to link to SDL to find its headers for the backends.
# This linking also makes SDL headers available to imgui sources during compilation.
# Ensure that the SDL3 target (e.g., SDL3::SDL3 or SDL3::main) is correctly defined by external/SDL/CMakeLists.txt
set_property(TARGET imgui PROPERTY CXX_STANDARD 11) # ImGui uses C++11, some backends might need it explicitly
target_link_libraries(imgui PUBLIC SDL3::SDL3)


# --- Source Files ---
set(APP_SOURCES
    src/main.cpp
    src/core/Application.cpp
    src/core/Config.cpp
    src/core/ControlSettings.cpp
    src/core/Events.cpp
    src/core/ImGuiManager.cpp
    src/core/InputActions.cpp
    src/core/Renderer.cpp
    src/core/SDLRenderer.cpp
    src/render/RenderManager.cpp
    src/render/RenderContext.cpp
    src/render/RenderPipeline.cpp
    src/view/Camera.cpp
    src/view/Viewport.cpp
    src/view/GridSettings.cpp
    src/view/Grid.cpp
    src/ui/MainMenuBar.cpp
    src/ui/windows/PCBViewerWindow.cpp
    src/ui/windows/SettingsWindow.cpp
    src/ui/interaction/InteractionManager.cpp
    src/ui/interaction/NavigationTool.cpp
    src/ui/windows/PcbDetailsWindow.cpp
    src/pcb/PcbLoader.cpp
    src/pcb/Board.cpp
    src/utils/des.cpp
    src/utils/StringUtils.cpp
    external/ImGuiFileDialog/ImGuiFileDialog.cpp
)

# --- Target Executable ---
add_executable(XZZPCB-Layer-Viewer ${APP_SOURCES})

# --- Include Directories ---
target_include_directories(XZZPCB-Layer-Viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuiFileDialog
    # ImGui includes are handled by linking with the imgui target
    # SDL includes are handled by linking with the SDL3::SDL3 target (via imgui or directly)
)

# --- Link Libraries ---
target_link_libraries(XZZPCB-Layer-Viewer PRIVATE
    imgui       # Our ImGui library target
    SDL3::SDL3  # The SDL3 library target from the submodule
    OpenGL::GL  # Explicitly link against OpenGL
)

# --- Platform Specifics (Optional) ---
if(APPLE)
    # macOS specific settings
endif()

if(MSVC)
    # Windows specific settings
else()
    # GCC/Clang specific settings
endif()

# Tests - commented out until we have a tests CMakeLists.txt
# if(BUILD_TESTS)
#     enable_testing()
#     add_subdirectory(tests)
# endif() 
cmake_minimum_required(VERSION 3.16)
project(PCBViewer LANGUAGES C CXX)

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

# Blend2D
# Add Blend2D as a subdirectory. Assumes it has its own CMakeLists.txt that creates a 'blend2d' target.
# Ensure your submodule is in external/blend2d.
add_subdirectory(external/blend2d EXCLUDE_FROM_ALL)

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

# --- Add source directory --- 
# This will process src/CMakeLists.txt, which should define the main executable
# and link against libraries built in src/core, src/render, etc.
add_subdirectory(src)

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
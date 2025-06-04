cmake_minimum_required(VERSION 3.21)

add_subdirectory(render) # Builds render_lib
add_subdirectory(view)   # Builds view_lib
add_subdirectory(ui)     # Builds ui_lib
add_subdirectory(core)   # Builds core_lib
add_subdirectory(utils)  # Builds utils_lib

set(EXECUTABLE_NAME XZZPCB-Layer-Viewer)
option(XZZPCBVIEWER_ENABLE_PCB_LOADER_LOGGING "Enable verbose logging for the PcbLoader" OFF)
set(ASMJIT_DIR ${CMAKE_SOURCE_DIR}/external/blend2d/3rdparty/asmjit)
set(MAIN_EXECUTABLE_SOURCES
    main.cpp
    # Include ImGuiFileDialog directly for now
    ${CMAKE_SOURCE_DIR}/external/ImGuiFileDialog/ImGuiFileDialog.cpp
)

# Create the main executable
add_executable(${EXECUTABLE_NAME} ${MAIN_EXECUTABLE_SOURCES})

# Link with our libraries and other dependencies
target_link_libraries(${EXECUTABLE_NAME}
    PRIVATE
    # Use WHOLE_ARCHIVE for ui_lib and core_lib to help with circular dependencies on MSVC
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,core_lib>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,ui_lib>"
    view_lib       # Library from src/view/
    render_lib     # Library from src/render/
    utils_lib      # Library from src/utils/
    imgui          # Defined in root CMakeLists.txt
    SDL3::SDL3     # From external/SDL submodule
    blend2d        # From external/blend2d submodule
    OpenGL::GL     # System OpenGL
)

# Set include directories for the executable
target_include_directories(${EXECUTABLE_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src  # To find headers in src/core, src/render, src/ui, etc.
    ${CMAKE_SOURCE_DIR}/external/ImGuiFileDialog # For ImGuiFileDialog.h
    # Include directories for imgui, SDL3, blend2d should be handled by linking
    # with their targets which propagate include directories.
)

if(XZZPCBVIEWER_ENABLE_PCB_LOADER_LOGGING)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ENABLE_PCB_LOADER_LOGGING)
endif()

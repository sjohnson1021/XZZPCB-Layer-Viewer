add_subdirectory(core) # Builds core_lib
add_subdirectory(render) # Builds render_lib
add_subdirectory(view)   # Builds view_lib

set(EXECUTABLE_NAME XZZPCB-Layer-Viewer)

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
    core_lib       # Library from src/core/
    render_lib     # Library from src/render/
    view_lib       # Library from src/view/
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
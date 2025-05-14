add_subdirectory(core)

set(SOURCE_FILES
    main.cpp
)

# Create the main executable
add_executable(pcb-viewer ${SOURCE_FILES})

# Link with the core library and other dependencies
target_link_libraries(pcb-viewer
    PRIVATE
    core
    imgui
    SDL3::SDL3
)

# Set include directories
target_include_directories(pcb-viewer
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
) 
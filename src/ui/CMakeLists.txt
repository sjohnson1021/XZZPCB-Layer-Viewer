cmake_minimum_required(VERSION 3.21)

set(LIBRARY_NAME ui_lib)

# UI components
set(SOURCE_FILES
    MainMenuBar.cpp
    windows/PCBViewerWindow.cpp
    windows/SettingsWindow.cpp
    windows/PcbDetailsWindow.cpp
    interaction/InteractionManager.cpp
    interaction/NavigationTool.cpp
)

add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})

target_include_directories(${LIBRARY_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/
)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
    view_lib
    render_lib
    SDL3::SDL3
    imgui
    PRIVATE
    core_lib
)

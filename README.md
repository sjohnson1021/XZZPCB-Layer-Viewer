# PCB Viewer

A simple, lightweight PCB viewer application for viewing and analyzing PCB files using SDL3, Blend2D and DearImGui, (and currently ImGuiFileDialog).

Hoping to optimize and clean up the rendering pipeline to be more easily adapted to other projects. 


# Credits

This project would not have been possible without the following people, projects, and libraries:

- [@slimeinacloak](https://github.com/slimeinacloak), Paul Daniels ([@inflex](https://github.com/inflex/), Muerto ([@MuertoGB](https://github.com/MuertoGB)), and others for the initial reverse engineering of the XZZ PCB file format. Major thanks to this team for their work, and for sharing their findings and code with the community.

- [XZZPCB-ImHex] (https://github.com/slimeinacloak/XZZPCB-ImHex) for the initial reverse engineering of the XZZ PCB file format and example implementation in OBV
- [Blend2D](https://github.com/blend2d/blend2d) for 2D rendering
- [ImGui](https://github.com/ocornut/imgui) for the user interface
- [ImGui-FileBrowser](https://github.com/AirGuanZ/imgui-filebrowser) for file browsing
- [SDL3](https://github.com/libsdl-org/SDL) for windowing and input

## Features

- Open and view XZZ format Layer PCB files
- Navigate and zoom PCB layouts
- Control layer visibility
- Analyze component connections
- Visualize trace layers

## Building

### Prerequisites

- CMake 3.21 or higher
- C++17 compatible compiler

### Build Instructions

#### Linux/MacOS

```bash
# Clone the repository
git clone https://github.com/yourusername/XZZPCB-Layer-Viewer.git
cd XZZPCB-Layer-Viewer

# Initialize and update submodules
git submodule update --init --recursive

# Blend2D requires asmjit, as a submodule/included in a 3rdparty/ subdirectory
cd external/blend2d/
mkdir 3rdparty
cd 3rdparty
git clone https://github.com/asmjit/asmjit ./asmjit/

#Back out to project root directory
cd ../../../

# Decide, whether you are using cmake alone, brew, or ninja

        # Ninja-Multi-Vcpkg
                # Configure
                cmake --preset=ninja-multi-vcpkg -DCMAKE_BUILD_TYPE=Release

                cd builds/ninja-multi-vcpkg
                ninja -f build-Release.ninja

        # CMake
                # Create a build directory
                mkdir build && cd build
                #Configure
                cmake -DCMAKE_BUILD_TYPE + RELEASE ..
                cmake --build .

        # Brew
                #TODO - Look at current GitHub CI implementation for MacOS build process, document here

# Run the application
./bin/XZZPCB-Layer-Viewer
```

#### Windows

```bash
# Clone the repository
git clone https://github.com/yourusername/XZZPCB-Layer-Viewer.git
cd XZZPCB-Layer-Viewer

# Initialize and update submodules
git submodule update --init --recursive

# Create a build directory
mkdir build
cd build

# Configure and build
cmake ..
cmake --build . --config Release

# Run the application
bin\Release\XZZPCB-Layer-Viewer.exe
```

## Development

This project follows a modular architecture with the following components:

- `core`: Core application framework
- `pcb`: PCB data model and loading
- `render`: Rendering system
- `ui`: User interface
- `view`: View and camera system

## License
# MIT License type - free to use and modify for personal or commercial, no strings

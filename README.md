# PCB Viewer

A clean, modular PCB viewer application for viewing and analyzing PCB files.

## Features

- Open and view XZZ format PCB files
- Navigate and zoom PCB layout
- Control layer visibility
- Analyze component connections
- View detailed component and pin information

## Building

### Prerequisites

- CMake 3.14 or higher
- C++17 compatible compiler
- SDL2 development libraries

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

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
git clone https://github.com/yourusername/pcb-viewer.git
cd pcb-viewer

# Initialize and update submodules
git submodule update --init --recursive

# Create a build directory
mkdir build && cd build

# Configure and build
cmake ..
make

# Run the application
./bin/pcb-viewer
```

#### Windows

```bash
# Clone the repository
git clone https://github.com/yourusername/pcb-viewer.git
cd pcb-viewer

# Initialize and update submodules
git submodule update --init --recursive

# Create a build directory
mkdir build
cd build

# Configure and build
cmake ..
cmake --build . --config Release

# Run the application
bin\Release\pcb-viewer.exe
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
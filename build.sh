#!/bin/bash

# Exit on error
set -e

# Create build directory if it doesn't exist
mkdir -p build

# Enter build directory
cd build

# Configure with CMake
cmake ..

# Build
cmake --build . -- -j$(nproc)

# Run the application if requested
if [ "$1" == "run" ]; then
    echo "Running PCB Viewer..."
    ./bin/pcb-viewer
fi 
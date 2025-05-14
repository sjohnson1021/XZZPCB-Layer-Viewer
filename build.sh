#!/bin/bash

# Exit on error
set -e

# Define the build type variable, default to Release if no argument is given
BUILD_TYPE="Release"

# Check if an argument is provided
if [ -n "$1" ]; then
    # If an argument is provided, set the build type
    if [ "$1" == "Debug" ] || [ "$1" == "Release" ]; then
        BUILD_TYPE="$1"
    else
        echo "Warning: Invalid build type '$1'. Defaulting to Release."
    fi
fi

# Create build directory if it doesn't exist
mkdir -p build

# Enter build directory
cd build

# Configure with CMake, using the determined build type
cmake -D CMAKE_BUILD_TYPE="$BUILD_TYPE" ..

# Build the project
cmake --build . -- -j$(nproc)


# Clean the build directory
make clean

#Rebuild
make

# Run the application if the first argument was 'run' or 'Debug'
if [ "$1" == "run" ] || [ "$BUILD_TYPE" == "Debug" ]; then
    echo "Running PCB Viewer..."
    ./bin/pcb-viewer
fi
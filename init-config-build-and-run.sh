#! /bin/bash

# Clone the repository

# git clone https://github.com/sjohnson1021/XZZPCB-Layer-Viewer.git
# cd XZZPCB-Layer-Viewer

# --Or if you already have the repository--

#

# Initialize submodules (SDL3, Blend2D) (Other includes are stubborn at the moment, and don't like being submodules)

git submodule update --init --recursive


# Blend2D requires AsmJit, and looks for it under 3rdparty.
# This is not included in the submodules, so we need to clone it first
cd external/blend2d
mkdir 3rdparty && cd 3rdparty
git clone https://github.com/asmjit/asmjit.git

# Back out to the project root
cd ../../../

# Configure with ninja-multi-vcpkg preset by default
cmake --preset=ninja-multi-vcpkg -DCMAKE_BUILD_TYPE=Release

# Enter build directory, and build with ninja
cd builds/ninja-multi-vcpkg
ninja -f build-Release.ninja

# Run the binary

cd bin/Release

./XZZPCB-Layer-Viewer

#cd ../../../build/

#cmake .. -DCMAKE_BUILD_TYPE=Release

#cmake --build . --config Release

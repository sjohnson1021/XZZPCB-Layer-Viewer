#!/bin/bash

cmake --preset=ninja-multi-vcpkg -DCMAKE_BUILD_TYPE=Release
cd builds/ninja-multi-vcpkg
ninja -f build-Release.ninja
cd bin/Release
./XZZPCB-Layer-Viewer

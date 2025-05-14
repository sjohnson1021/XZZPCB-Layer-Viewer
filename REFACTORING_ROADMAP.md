# PCB Viewer Project Roadmap

## Overview
This document outlines the plan for creating a new PCB Viewer application with a clean, modular architecture from the ground up. Rather than refactoring the existing monolithic codebase, we'll build a new application that implements the same functionality with a proper architecture.

## Project Structure
```
pcb-viewer/
├── CMakeLists.txt
├── src/
│   ├── core/        # Core application framework
│   ├── pcb/         # PCB data model and loading
│   ├── render/      # Rendering system
│   ├── ui/          # User interface
│   ├── view/        # View and camera system
│   └── main.cpp     # Application entry point
├── external/        # External dependencies
│   ├── imgui/
│   ├── blend2d/
│   └── imgui-filebrowser/
├── tests/           # Unit tests
└── .github/         # GitHub workflow configurations
```

### Git Development Workflow
1. **Use feature branches for development**:
   ```bash
   git checkout -b feature/component-name
   # Make changes
   git add .
   git commit -m "Implement feature X"
   git push -u origin feature/component-name
   ```

2. **Create pull requests** for code review before merging to main.

3. **Tag releases** using semantic versioning:
   ```bash
   git tag -a v0.1.0 -m "Initial release"
   git push origin v0.1.0
   ```

4. **Automate releases** using GitHub Actions by creating a release workflow that builds and packages the application when a tag is pushed.

### Minimum Viable Project
1. **Create a minimal application** that shows a window with basic ImGui integration.

2. **Implement a simple rendering pipeline** with SDL and Blend2D.

3. **Set up tests** to verify core functionality.

4. **Commit and push** to establish the baseline for further development.


### For Each Phase
1. Design interfaces and class hierarchies
2. Implement minimal working functionality
3. Test and verify before moving to the next component
4. Document code with clear comments
5. Create examples of usage

### Development Workflow
1. Use feature branches for each component
2. Review and refactor code regularly
3. Maintain code style consistency
4. Commit frequently with clear messages
5. Create checkpoints after completing each phase



## Build System and Development Environment

### CMake Configuration
1. **Build Configuration**
   - `CMakeLists.txt`: Main build configuration
   - `src/CMakeLists.txt`: Source-specific build configuration
   - `tests/CMakeLists.txt`: Test-specific build configuration

2. **Build Profiles**
   - Debug build with full symbols (`-g3 -O0 -fno-omit-frame-pointer`)
   - Release build with optimizations (`-O3 -march=native -flto -ffast-math`)
   - Profile build for performance analysis
   - Sanitizer builds (Address, UB, Thread) for enhanced debugging

3. **External Dependencies Management**
   - Consistent submodule management
   - Version pinning for dependencies
   - Fallback mechanisms for system vs. bundled libraries

### Development Tools Integration
1. **IDE Configuration**
   - `.vscode/`: VS Code configuration files
     - `tasks.json`: Build and run tasks
     - `launch.json`: Debug configurations
     - `settings.json`: Editor settings
     - `extensions.json`: Recommended extensions

2. **Build Scripts**
   - `scripts/build.sh`: Common build script for local development
   - `scripts/debug.sh`: Debug script with common debug flags
   - `scripts/profile.sh`: Performance profiling script

## Implementation Strategy

### Development Approach
1. **Incremental Development**
   - Add components incrementally, testing each step
   - Build up functionality layer by layer
   - Maintain a working application at all times

2. **Component-Based Design**
   - Use clear separation of concerns
   - Design clean interfaces between components
   - Use dependency injection for loose coupling
   - Implement proper encapsulation

3. **Testing Strategy**
   - Create unit tests for core functionality
   - Add integration tests for component interactions
   - Implement visual tests for rendering components
   - Test with real PCB files regularly (When we get to the end of Phase 3)

### CI/CD Integration
1. **GitHub Workflows**
   - `.github/workflows/build.yml`: Main build workflow
     - Matrix build across platforms (Windows, macOS, Linux)
     - Build with multiple compiler versions
     - Matching local and CI build flags

2. **Testing Framework**
   - `.github/workflows/test.yml`: Testing workflow
     - Unit tests
     - Integration tests
     - UI tests

3. **Release Automation**
   - `.github/workflows/release.yml`: Release workflow
     - Version tagging
     - Binary packaging
     - Release notes generation

### Documentation
1. **Build Documentation**
   - `docs/build.md`: Build system documentation
   - `docs/dependencies.md`: Dependencies documentation

2. **Developer Guides**
   - `docs/developer/setup.md`: Development environment setup
   - `docs/developer/workflow.md`: Development workflow

The build system should ensure consistent behavior between local development and CI/CD environments, with parallel configurations for different build types (Debug/Release) and proper optimization flags based on the build type. The VS Code tasks and launch configurations should align with the CI/CD workflows to ensure developers can easily reproduce build and test environments locally.

## Phase 2: View and Camera System
**Goal**: Implement base of rendering framework (if we need it at the moment for SDL and ImGui. If not, and we only need to worry about the rendering pipeline for Blend2D, then we can safely ignore this for now, otherwise we should likely go ahead and set up this manager/context/pipeline set.), view management and camera controls, and prepare base of interaction manager, to allow for panning and zooming, either via clicking and dragging, and scrolling, or keyboard shortcuts, or a having either as an option in addition to on-screen buttons for at least resetting the view (centering around content) and zooming in and out. 

### Components to Implement:
1. **Rendering Framework**
   - `src/render/RenderManager.hpp/cpp`: Main rendering coordinator
   - `src/render/RenderContext.hpp/cpp`: Rendering context and state
   - `src/render/RenderPipeline.hpp/cpp`: Rendering pipeline and stages
2. **Camera System**
   - `src/view/Camera.hpp/cpp`: Camera management (pan, zoom, transform)
   - `src/view/Viewport.hpp/cpp`: Viewport management and coordinate conversion
   - `src/view/CoordinateSystem.hpp/cpp`: World/screen coordinate conversion

3. **Grid System**
   - `src/view/Grid.hpp/cpp`: Grid rendering and management
   - `src/view/GridSettings.hpp/cpp`: Grid configuration

### UI to Implement:
1. **UI Windows**
   - `src/ui/windows/PCBViewerWindow.hpp/cpp`: PCB viewing window
   - `src/ui/windows/SettingsWindow.hpp/cpp`: Settings window

2. **Interaction System**
   - `src/ui/interaction/InteractionManager.hpp/cpp`: Interaction coordination
   - `src/ui/interaction/NavigationTool.hpp/cpp`: Navigation tool (pan/zoom)

### Implementation Tasks:
- Implement camera controls (pan, zoom)
- Create coordinate transformation utilities
- Implement grid rendering with various scale levels
- Add grid configuration options
- Create a test scene to verify camera and grid functionality
- Implement UI window framework
- Create application UI layout
- Add navigation tools and shortcuts
**Build Goal: Render configurable grid, and allow for mouse/keyboard viewport interactions (pan, zoom, rotate(?)**




///NEXT STEPS


### BUILD AND COMMIT (on successful build) CHECKPOINT

## Phase 3: PCB Data Model
**Goal**: Implement proper PCB data representation and loading.

### Components to Implement:
1. **PCB Data Structures**
   - `src/pcb/PCBData.hpp/cpp`: Main PCB data container
   - `src/pcb/Elements.hpp/cpp`: Base element class and common functionality

2. **PCB Element Types**
   - `src/pcb/elements/Trace.hpp/cpp`: Trace implementation
   - `src/pcb/elements/Via.hpp/cpp`: Via implementation
   - `src/pcb/elements/Component.hpp/cpp`: Component implementation
   - `src/pcb/elements/Pin.hpp/cpp`: Pin implementation
   - `src/pcb/elements/Arc.hpp/cpp`: Arc implementation
   - `src/pcb/elements/Text.hpp/cpp`: Text implementation

3. **PCB File Loading**
   - `src/pcb/PCBLoader.hpp/cpp`: Abstract PCB loader interface
   - `src/pcb/XZZPCBLoader.hpp/cpp`: XZZ PCB file format loader
   - `src/pcb/FileFormats.hpp/cpp`: File format utilities and helpers

### Implementation Tasks:
- Design clean object-oriented PCB data model
- Implement element classes with proper encapsulation
- Create file loading infrastructure
- Implement XZZ PCB file format loader
- Add proper error handling and validation
- Create test cases for loading and data integrity
**Build Goal: Successfully load PCB files, view file details in a debug window, no rendering yet**
### BUILD AND COMMIT (on successful build) CHECKPOINT


## Phase 4: Rendering System
**Goal**: Implement modular PCB rendering system.

### Components to Implement:


2. **Element Renderers**
   - `src/render/ElementRenderer.hpp/cpp`: Base element renderer
   - `src/render/renderers/TraceRenderer.hpp/cpp`: Trace rendering
   - `src/render/renderers/ViaRenderer.hpp/cpp`: Via rendering
   - `src/render/renderers/ComponentRenderer.hpp/cpp`: Component rendering
   - `src/render/renderers/PinRenderer.hpp/cpp`: Pin rendering
   - `src/render/renderers/ArcRenderer.hpp/cpp`: Arc rendering
   - `src/render/renderers/TextRenderer.hpp/cpp`: Text rendering

3. **Layer Management**
   - `src/render/LayerManager.hpp/cpp`: Layer visibility and ordering
   - `src/render/LayerSettings.hpp/cpp`: Layer appearance settings

### Implementation Tasks:
- Create rendering framework with proper abstraction
- Implement element renderers for each element type
- Add layer management and visibility control
- Implement proper rendering pipeline
- Optimize rendering for performance
- Add configurable rendering settings

## Phase 5: UI System
**Goal**: Implement proper UI and interaction system.

### Components to Implement:

1. **UI Windows**
   - `src/ui/windows/DebugWindow.hpp/cpp`: Debug information window
   - `src/ui/windows/LayerControlWindow.hpp/cpp`: Layer control window

2. **Interaction System**
   - `src/ui/interaction/SelectionManager.hpp/cpp`: Selection management
   - `src/ui/interaction/HoverManager.hpp/cpp`: Hover detection and highlighting

### Implementation Tasks:

- Add layer control and settings UI
- Implement selection and hover functionality
- Create user-friendly UI with proper labels and tooltips

## Phase 6: Debug and Development Tools
**Goal**: Implement development and debugging tools.

### Components to Implement:
1. **Debug Framework**
   - `src/debug/DebugManager.hpp/cpp`: Debug system management
   - `src/debug/DebugRenderer.hpp/cpp`: Debug visualization
   - `src/debug/PerformanceMonitor.hpp/cpp`: Performance monitoring

2. **Debug Tools**
   - `src/debug/tools/ElementInspector.hpp/cpp`: Element inspection tool
   - `src/debug/tools/LayerDebugger.hpp/cpp`: Layer debugging tool
   - `src/debug/tools/MetricsWindow.hpp/cpp`: Performance metrics window

### Implementation Tasks:
- Implement debug visualization system
- Add element inspection tools
- Create performance monitoring
- Add debug UI windows
- Implement debugging utilities for development



## Migration Strategy
Once the new application reaches feature parity with the existing one:
1. Test with the same PCB files used in the current application
2. Compare performance and functionality
3. Address any missing features or differences
4. Create user documentation for the new application
5. Phase out the old application

## Future Considerations
- Add unit testing framework
- Implement CI/CD pipeline
- Add more PCB file format support
- Create plugin system for extensibility
- Add advanced features (measurements, net tracing, etc.)

## Additional Considerations for Roadmap

### PCB File Format Support
1. **XZZ PCB File Format Handling**
   - `src/pcb/formats/XZZPCBLoader.hpp/cpp`: Specific implementation for handling XZZ PCB files
   - `src/pcb/encryption/DES.hpp/cpp`: Encryption/decryption utilities for protected files
   - `src/pcb/encoding/CharsetConverter.hpp/cpp`: Support for various character encodings (UTF-8, CB2312)

2. **File Format Utilities**
   - `src/pcb/utils/ByteUtils.hpp/cpp`: Utilities for handling binary data and endianness
   - `src/pcb/validation/FormatValidator.hpp/cpp`: Validation for file integrity and format compliance

### Advanced Rendering Features
1. **Pin Orientation and Rendering**
   - `src/render/pin/PinOrientationDetector.hpp/cpp`: Intelligent pin orientation detection for components
   - `src/render/pin/PinStyleManager.hpp/cpp`: Pin styling and appearance configuration
   
2. **Net Highlighting and Tracing**
   - `src/render/net/NetHighlighter.hpp/cpp`: Net highlighting capability
   - `src/render/net/NetTracer.hpp/cpp`: Trace nets across the board for continuity analysis

3. **Blend2D Integration**
   - `src/render/blend2d/BLPathCache.hpp/cpp`: Path caching for optimized rendering
   - `src/render/blend2d/BLStyleManager.hpp/cpp`: Styling and appearance for Blend2D rendering

### PCB Data Model Extensions
1. **Component Analysis and Organization**
   - `src/pcb/analysis/ComponentClassifier.hpp/cpp`: Component type identification (QFP, connector, etc.)
   - `src/pcb/analysis/PinClassifier.hpp/cpp`: Pin classification for orientation detection

2. **Layer Data Management**
   - `src/pcb/layer/LayerStack.hpp/cpp`: Layer organization and ordering
   - `src/pcb/layer/LayerProperties.hpp/cpp`: Layer-specific properties and appearance

### Viewer Functionality
1. **Board Folding and Flipping**
   - `src/view/BoardFolder.hpp/cpp`: Logic for folding the board display (de-butterfly)
   - `src/view/BoardFlipper.hpp/cpp`: Board mirroring and flipping capabilities

2. **Mouse Interaction and Selection**
   - `src/ui/interaction/ElementSelector.hpp/cpp`: Element selection and hover detection
   - `src/ui/interaction/MouseInteractionManager.hpp/cpp`: Mouse interaction handling

### Debug and Development Features
1. **Component and Pin Details Display**
   - `src/ui/windows/ElementInspectorWindow.hpp/cpp`: Detailed element property display
   - `src/ui/windows/ComponentDetailsWindow.hpp/cpp`: Component details and pin information

2. **Performance and Optimization**
   - `src/core/profiler/RenderProfiler.hpp/cpp`: Performance monitoring for rendering
   - `src/core/profiler/MemoryTracker.hpp/cpp`: Memory usage tracking

### Data Export and Persistence
1. **Settings Management**
   - `src/core/settings/AppSettings.hpp/cpp`: Application settings persistence
   - `src/core/settings/ViewSettings.hpp/cpp`: View state persistence

2. **Data Export**
   - `src/export/ImageExporter.hpp/cpp`: Export board view as image
   - `src/export/MeasurementExporter.hpp/cpp`: Export measurements and annotations


   ////DONE


## Project Setup and Git Workflow
git 
   ```

2. **Set up initial project structure**:
   ```bash
   mkdir -p src/{core,pcb,render,ui,view} external tests .github/workflows
   touch CMakeLists.txt .gitignore README.md
   ```

3. **Create a comprehensive .gitignore file** for C++ projects:
   ```
   # Build directories
   build/
   out/
   
   # Compiled objects
   *.o
   *.so
   *.a
   *.dll
   *.exe
   
   # CMake files
   CMakeCache.txt
   CMakeFiles/
   cmake_install.cmake
   
   # IDE files
   .vscode/
   .idea/
   *.swp
   
   # OS files
   .DS_Store
   ```

4. **Create base CMakeLists.txt** for cross-platform building:
   ```cmake
   cmake_minimum_required(VERSION 3.14)
   project(pcb-viewer VERSION 0.1.0 LANGUAGES CXX)
   
   set(CMAKE_CXX_STANDARD 17)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
   
   # Options
   option(BUILD_TESTS "Build tests" ON)
   
   # Organize output files
   set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
   set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
   set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
   
   # Source files
   add_subdirectory(src)
   
   # Tests
   if(BUILD_TESTS)
       enable_testing()
       add_subdirectory(tests)
   endif()
   ```

5. **Create a README.md** with build instructions and project overview.

### External Dependencies Management
1. **Set up Git submodules for external libraries**:
   ```bash
   git submodule add https://github.com/libsdl-org/SDL.git external/SDL
   git submodule add https://github.com/ocornut/imgui.git external/imgui
   git submodule add https://github.com/blend2d/blend2d.git external/blend2d
   # Add other dependencies as needed
   ```

2. **Configure CMake to use external dependencies** by adding appropriate include and link commands.

### Continuous Integration Setup
1. **Create GitHub Actions workflow file** in `.github/workflows/ci.yml`:
   ```yaml
   name: CI

   on:
     push:
       branches: [ main ]
     pull_request:
       branches: [ main ]
   
   jobs:
     build:
       name: ${{ matrix.config.name }}
       runs-on: ${{ matrix.config.os }}
       strategy:
         fail-fast: false
         matrix:
           config:
           - {
               name: "Windows MSVC",
               os: windows-latest,
               build_type: "Release", cc: "cl", cxx: "cl"
             }
           - {
               name: "Ubuntu GCC",
               os: ubuntu-latest,
               build_type: "Release", cc: "gcc", cxx: "g++"
             }
           - {
               name: "macOS Clang",
               os: macos-latest,
               build_type: "Release", cc: "clang", cxx: "clang++"
             }
   
       steps:
       - uses: actions/checkout@v3
         with:
           submodules: true
   
       - name: Install dependencies (Ubuntu)
         if: matrix.config.os == 'ubuntu-latest'
         run: |
           sudo apt-get update
           sudo apt-get install -y libsdl2-dev
   
       - name: Configure CMake
         run: |
           mkdir build
           cd build
           cmake .. -DCMAKE_BUILD_TYPE=${{ matrix.config.build_type }}
   
       - name: Build
         run: |
           cd build
           cmake --build . --config ${{ matrix.config.build_type }}
   
       - name: Test
         run: |
           cd build
           ctest -C ${{ matrix.config.build_type }}
   ```



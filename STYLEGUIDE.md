# XZZPCB Layer Viewer - Style Guide

Welcome to the XZZPCB Layer Viewer project! This document outlines the coding standards and conventions used throughout the codebase. Following these guidelines ensures consistency, maintainability, and ease of collaboration.

## Table of Contents
- [Project Overview](#project-overview)
- [Build System](#build-system)
- [Naming Conventions](#naming-conventions)
- [File Organization](#file-organization)
- [Code Formatting](#code-formatting)
- [Modern C++ Standards](#modern-c-standards)
- [Documentation](#documentation)
- [Error Handling](#error-handling)
- [Performance Guidelines](#performance-guidelines)
- [Testing Standards](#testing-standards)
- [Git Workflow](#git-workflow)
- [Tools and Setup](#tools-and-setup)

## Project Overview

XZZPCB Layer Viewer is a cross-platform PCB visualization tool built with modern C++ (C++17/20), using CMake for build configuration, Ninja for compilation, and vcpkg for dependency management.

### Key Technologies
- **Language**: C++17/20
- **Build System**: CMake + Ninja
- **Package Manager**: vcpkg
- **Target Platforms**: Windows, Linux, macOS
- **Primary Use Case**: PCB layer visualization and analysis

## Build System

### Project Structure
```
xzzpcb-layer-viewer/
├── CMakeLists.txt
├── vcpkg.json
├── src/
│   ├── core/           # Core PCB data structures
│   ├── rendering/      # Visualization and rendering
│   ├── io/            # File I/O operations
│   └── utils/         # Utility functions
├── include/
│   └── xzzpcb/        # Public headers
├── tests/
├── docs/
└── examples/
```

### CMake Conventions
- Use `snake_case` for target names: `xzzpcb_core`, `xzzpcb_rendering`
- Organize targets by logical modules
- Always specify C++ standard explicitly
- Use modern CMake practices (target-based, not directory-based)

```cmake
add_library(xzzpcb_core STATIC
    src/core/layer.cpp
    src/core/pcb_data.cpp
)

set_target_properties(xzzpcb_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
```

### vcpkg Dependencies
All external dependencies must be managed through vcpkg. Update `vcpkg.json` when adding new dependencies:

```json
{
    "name": "xzzpcb-layer-viewer",
    "version-string": "1.0.0",
    "dependencies": [
        "fmt",
        "spdlog",
        "nlohmann-json"
    ]
}
```

## Naming Conventions

### Types and Classes
- **Classes/Structs**: `PascalCase`
  ```cpp
  class LayerRenderer { };
  struct PCBData { };
  ```

- **Enums**: `PascalCase` with scoped enums preferred
  ```cpp
  enum class LayerType { Copper, SolderMask, Drill };
  ```

- **Type aliases**: `PascalCase`
  ```cpp
  using LayerMap = std::unordered_map<int, Layer>;
  ```

### Functions and Variables
- **Functions**: `PascalCase` for public methods, `camelCase` for private methods
  ```cpp
  class LayerRenderer {
  public:
      void RenderLayer(const Layer& layer);  // Public: PascalCase
  private:
      void setupRenderState();               // Private: camelCase
  };
  ```

- **Variables**: `snake_case` for all variables
  ```cpp
  int layer_count = 0;
  std::string file_path = "data.pcb";
  ```

- **Member variables**: `snake_case_` with trailing underscore
  ```cpp
  class PCBViewer {
  private:
      std::vector<Layer> layers_;
      bool is_initialized_{false};
  };
  ```

- **Constants**: `kPascalCase`
  ```cpp
  constexpr int kMaxLayerCount = 64;
  constexpr double kDefaultScale = 1.0;
  ```

- **Macros**: `SCREAMING_SNAKE_CASE` (avoid when possible)
  ```cpp
  #define XZZPCB_VERSION_MAJOR 1
  ```

### Files and Directories
- **Source files**: `snake_case.cpp`
- **Header files**: `snake_case.h`
- **Directories**: `snake_case`

```
src/layer_renderer.cpp
include/xzzpcb/pcb_viewer.h
tests/rendering_tests/
```

## File Organization

### Header Files (.h)
```cpp
#pragma once

// Standard library includes (alphabetical)
#include <memory>
#include <string>
#include <vector>

// Third-party includes (alphabetical)
#include <fmt/core.h>
#include <spdlog/spdlog.h>

// Project includes (alphabetical, relative paths, can be relative to current source file)
#include "core/layer.h"
#include "rendering/renderer_interface.h"

// in ./src/view/Camera.cpp
#include "Camera.hpp"

namespace xzzpcb {

/**
 * @brief Brief description of the class purpose.
 *
 * Detailed description of what this class does, its responsibilities,
 * and any important usage notes.
 */
class ClassName {
public:
    // Public types and constants
    enum class Status { Success, Error };
    static constexpr int kMaxItems = 100;

    // Constructors/Destructor
    explicit ClassName(const std::string& name);
    ClassName(const ClassName&) = delete;
    ClassName& operator=(const ClassName&) = delete;
    ClassName(ClassName&&) noexcept = default;
    ClassName& operator=(ClassName&&) noexcept = default;
    ~ClassName() = default;

    // Public methods (grouped logically)
    void Initialize();
    bool ProcessData(const std::vector<int>& data);

    // Getters/Setters
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

private:
    // Private methods
    void validateInput(const std::vector<int>& data) const;
    bool processInternal();

    // Member variables (grouped logically)
    std::string name_;
    std::vector<int> data_;
    bool is_initialized_{false};
};

}  // namespace xzzpcb
```

### Source Files (.cpp)
```cpp
#include "xzzpcb/path/to/header.h"

// Standard library includes
#include <algorithm>
#include <stdexcept>

// Third-party includes
#include <spdlog/spdlog.h>

// Project includes
#include "xzzpcb/utils/logging.h"

namespace xzzpcb {

ClassName::ClassName(const std::string& name) : name_(name) {
    spdlog::debug("Creating ClassName with name: {}", name);
}

void ClassName::Initialize() {
    if (is_initialized_) {
        spdlog::warn("ClassName already initialized");
        return;
    }

    // Implementation here
    is_initialized_ = true;
}

// Private methods implementation
void ClassName::validateInput(const std::vector<int>& data) const {
    if (data.empty()) {
        throw std::invalid_argument("Input data cannot be empty");
    }
}

}  // namespace xzzpcb
```

## Code Formatting

### Basic Rules
- **Indentation**: 4 spaces (no tabs)
- **Line length**: Maximum 100 characters
- **Braces**: Opening brace on same line (K&R style)
- **Spacing**: Single space around operators, after commas

### Examples
```cpp
// Control flow
if (condition) {
    DoSomething();
} else if (other_condition) {
    DoSomethingElse();
} else {
    DoDefault();
}

// Loops - prefer range-based when possible
for (const auto& layer : layers_) {
    ProcessLayer(layer);
}

// Traditional loops when index needed
for (size_t i = 0; i < container.size(); ++i) {
    ProcessAtIndex(container[i], i);
}

// Function calls with many parameters
auto result = ComplexFunction(
    parameter_one,
    parameter_two,
    parameter_three);

// Method chaining
builder.SetWidth(800)
       .SetHeight(600)
       .SetTitle("PCB Viewer")
       .Build();
```

### Pointer and Reference Declarations
```cpp
// Prefer: asterisk/ampersand with type
int* pointer;
const std::string& reference;
std::unique_ptr<Layer> layer_ptr;
```

## Modern C++ Standards

### Required Practices
- Use `nullptr` instead of `NULL` or `0` for pointers
- Use `override` and `final` for virtual functions
- Prefer `enum class` over plain `enum`
- Use `const` extensively
- Use `constexpr` for compile-time constants
- Prefer smart pointers over raw pointers for ownership

### Preferred Patterns
```cpp
// Auto type deduction (when type is obvious)
auto layer = std::make_unique<CopperLayer>();
auto it = layers_.find(layer_id);

// Range-based algorithms
std::sort(layers_.begin(), layers_.end(),
          [](const Layer& a, const Layer& b) {
              return a.GetZOrder() < b.GetZOrder();
          });

// Structured bindings (C++17)
if (auto [iter, inserted] = layer_map.emplace(id, layer); inserted) {
    // Handle successful insertion
}

// Optional for potentially missing values
std::optional<Layer> FindLayerByName(const std::string& name) const;

// String view for read-only string parameters
void ProcessLayerName(std::string_view name);
```

### Memory Management
```cpp
// Prefer smart pointers
std::unique_ptr<Renderer> CreateRenderer();
std::shared_ptr<TextureCache> GetCache();

// Use make_unique/make_shared
auto renderer = std::make_unique<OpenGLRenderer>(config);
auto cache = std::make_shared<LayerCache>();

// RAII for resource management
class FileHandler {
public:
    explicit FileHandler(const std::string& filename);
    ~FileHandler();  // Automatic cleanup
private:
    FILE* file_handle_;
};
```

## Documentation

### Class and Function Documentation
Use Doxygen-style comments for all public APIs:

```cpp
/**
 * @brief Renders PCB layers using OpenGL.
 *
 * This class provides high-performance rendering of PCB layer data using
 * modern OpenGL techniques. Supports multiple layer types including copper,
 * solder mask, and drill layers.
 *
 * @note Thread-safe for read operations only. Write operations require
 *       external synchronization.
 */
class LayerRenderer {
public:
    /**
     * @brief Renders the specified layer to the framebuffer.
     *
     * @param layer_id Unique identifier for the layer to render
     * @param transform Transformation matrix for rendering
     * @return true if rendering succeeded, false otherwise
     *
     * @throws LayerNotFoundException if layer_id is invalid
     * @throws RenderException if OpenGL operations fail
     */
    bool RenderLayer(int layer_id, const Transform& transform);
};
```

### Inline Comments
```cpp
void ProcessLayers() {
    // Sort layers by Z-order for proper depth rendering
    std::sort(layers_.begin(), layers_.end(),
              [](const Layer& a, const Layer& b) {
                  return a.GetZOrder() < b.GetZOrder();
              });

    // TODO(username): Implement layer caching for better performance
    // FIXME: Handle empty layer collection properly
    for (const auto& layer : layers_) {
        if (!layer.IsVisible()) {
            continue;  // Skip invisible layers
        }

        RenderLayer(layer);
    }
}
```

## Error Handling

### Exception Strategy
```cpp
// Project-specific exception hierarchy
class XZZPCBException : public std::runtime_error {
public:
    explicit XZZPCBException(const std::string& message)
        : std::runtime_error(message) {}
};

class LayerNotFoundException : public XZZPCBException {
public:
    explicit LayerNotFoundException(int layer_id)
        : XZZPCBException(fmt::format("Layer {} not found", layer_id)) {}
};

// Use specific exceptions
Layer& GetLayer(int layer_id) {
    auto it = layers_.find(layer_id);
    if (it == layers_.end()) {
        throw LayerNotFoundException(layer_id);
    }
    return it->second;
}
```

### Optional Return Values
```cpp
// Use std::optional for operations that may fail
std::optional<Layer> FindLayerByName(const std::string& name) const {
    auto it = std::find_if(layers_.begin(), layers_.end(),
                          [&name](const auto& layer) {
                              return layer.GetName() == name;
                          });

    return it != layers_.end() ? std::make_optional(*it) : std::nullopt;
}

// Usage
if (auto layer = FindLayerByName("copper_top")) {
    ProcessLayer(*layer);
} else {
    spdlog::warn("Layer 'copper_top' not found");
}
```

## Performance Guidelines

### General Principles
- Measure before optimizing
- Prefer standard library algorithms
- Avoid premature optimization
- Use appropriate data structures
- Consider cache locality

### Common Optimizations
```cpp
// Reserve capacity for vectors when size is known
std::vector<Layer> layers;
layers.reserve(expected_count);

// Use emplace over push for complex objects
layers.emplace_back(layer_id, layer_type, layer_data);

// Prefer const references for large objects
void ProcessLayer(const Layer& layer);  // Not: void ProcessLayer(Layer layer);

// Use move semantics appropriately
class LayerData {
public:
    explicit LayerData(std::vector<Point> points)
        : points_(std::move(points)) {}  // Move, don't copy
};
```

## Testing Standards

### Unit Tests
- Use a modern testing framework (catch2, gtest, etc.)
- Test public interfaces thoroughly
- Include edge cases and error conditions
- Maintain test isolation

```cpp
TEST_CASE("LayerRenderer can render copper layers", "[rendering]") {
    LayerRenderer renderer;
    CopperLayer layer(/* params */);

    REQUIRE(renderer.Initialize());
    REQUIRE(renderer.RenderLayer(layer.GetId(), Transform::Identity()));
}
```

### Integration Tests
- Test cross-module functionality
- Verify file I/O operations
- Test rendering pipeline end-to-end

## Git Workflow

### Commit Messages
Follow conventional commit format:
```
type(scope): description

[optional body]

[optional footer]
```

Examples:
```
feat(rendering): add support for drill layer visualization
fix(io): handle malformed PCB files gracefully
docs(api): update layer rendering documentation
refactor(core): simplify layer data structure
```

### Branch Naming
- `feature/description-of-feature`
- `bugfix/issue-description`
- `refactor/component-name`
- `docs/documentation-update`

### Pull Request Guidelines
- Include clear description of changes
- Reference any related issues
- Ensure all tests pass
- Update documentation if needed
- Follow code review feedback promptly

## Tools and Setup

### Required Tools
- **Compiler**: GCC 9+, Clang 10+, or MSVC 2019+
- **CMake**: 3.16 or later
- **vcpkg**: Latest version
- **Git**: Any recent version

### Recommended Development Environment
- **IDEs**: Visual Studio Code with C++ extensions, CLion, Visual Studio
- **Code Formatting**: clang-format (configuration provided in `.clang-format`)
- **Static Analysis**: clang-tidy, cppcheck
- **Debugging**: gdb, lldb, Visual Studio debugger

### Setting Up Development Environment
```bash
# Clone the repository
git clone https://github.com/your-org/xzzpcb-layer-viewer.git
cd xzzpcb-layer-viewer

# Initialize vcpkg (if not already done)
git submodule update --init --recursive

cd external/blend2d/
mkdir 3rdparty && cd 3rdparty
git clone https://github.com/asmjit/asmjit ./asmjit/

#Back out to project root
cd ../../../

# Configure build
cmake --preset ninja-multi-vcpkg

# Build
cmake --build --preset ninja-multi-vcpkg-debug

# Run tests
ctest --preset ninja-multi-vcpkg-debug
```

## Code Review Checklist

Before submitting code for review, ensure:

- [ ] Code follows naming conventions
- [ ] Functions are documented with Doxygen comments
- [ ] No compiler warnings
- [ ] Tests pass locally
- [ ] Code is formatted with clang-format
- [ ] No memory leaks (verified with valgrind/sanitizers)
- [ ] Error handling is appropriate
- [ ] Performance implications considered
- [ ] Documentation updated if needed

## Questions and Support

For questions about this style guide or coding standards:
- Create an issue in the project repository
- Start a discussion in the project's discussion forum
- Contact the maintainers directly

Thank you for contributing to XZZPCB Layer Viewer!

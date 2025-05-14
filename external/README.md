# External Dependencies

This directory contains external dependencies used by the PCB Viewer application. These dependencies are managed as Git submodules.

## Dependencies

- **ImGui**: Dear ImGui is a bloat-free graphical user interface library for C++. It outputs optimized vertex buffers that you can render anytime in your 3D-pipeline enabled application.
  - Repository: https://github.com/ocornut/imgui

- **Blend2D**: Blend2D is a 2D vector graphics engine written in C++ that uses JIT compilation to accelerate rendering.
  - Repository: https://github.com/blend2d/blend2d

- **ImGui-FileBrowser**: A file browser implementation for Dear ImGui.
  - Repository: https://github.com/AirGuanZ/imgui-filebrowser

## Adding Dependencies

To add the dependencies as submodules, run:

```bash
git submodule add https://github.com/ocornut/imgui.git external/imgui
git submodule add https://github.com/blend2d/blend2d.git external/blend2d
git submodule add https://github.com/AirGuanZ/imgui-filebrowser.git external/imgui-filebrowser
```

## Updating Dependencies

To update all submodules to their latest versions:

```bash
git submodule update --remote
```

## Initializing Dependencies

When cloning the repository, initialize and update submodules with:

```bash
git submodule update --init --recursive
``` 
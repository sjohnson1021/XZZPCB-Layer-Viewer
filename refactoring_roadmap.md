# Project Refactoring Roadmap

This document outlines the step-by-step plan to refactor the XZZPCB Layer Viewer codebase to align with the proposed architecture. The goal is to improve separation of concerns, clarify class responsibilities, and establish a robust foundation for future development, including the Blend2D rendering pipeline.

## Phase 1: Core Application Structure & UI Orchestration

**Objective:** Centralize application control within the `Application` class and clarify UI management.

1.  **Refactor `main.cpp` to Use `Application` Class:**
    *   **File:** `src/main.cpp`
    *   **Task:**
        *   Remove direct SDL and ImGui initialization, event loop, and shutdown logic.
        *   Instantiate `Application`.
        *   Call `app->Initialize()`, `app->Run()`, and `app->Shutdown()`.
    *   **Rationale:** Centralizes application lifecycle management in `Application`, making `main.cpp` a simple entry point.

2.  **Integrate `Renderer` and `ImGuiManager` into `Application` Lifecycle:**
    *   **Files:** `src/core/Application.hpp`, `src/core/Application.cpp`
    *   **Task:**
        *   In `Application::InitializeSubsystems()` (or `Application::Initialize()`):
            *   Initialize `m_renderer` using `Renderer::Create()` and `m_renderer->Initialize()`.
            *   Initialize `m_imguiManager`, passing it the `SDL_Window*` and `SDL_Renderer*` from `m_renderer` (via `GetWindowHandle()`/`GetRendererHandle()` and `static_cast`).
        *   In `Application::Shutdown()`:
            *   Call `m_imguiManager->Shutdown()` then `m_renderer->Shutdown()`.
        *   Ensure `m_config` is loaded before renderer/window initialization to potentially use configured settings.
    *   **Rationale:** `Application` correctly owns and manages these core subsystems.

3.  **Centralize Event Handling in `Application` & `Events`:**
    *   **Files:** `src/core/Application.cpp`, `src/core/Events.cpp`
    *   **Task:**
        *   `Application::ProcessEvents()`: Should primarily call `m_events->ProcessEvents()`.
        *   `Events::ProcessEvents()`: Should handle SDL event polling, pass events to `m_imguiManager->ProcessEvent()`, and manage `m_shouldQuit`.
    *   **Rationale:** Consolidates event processing flow as per the diagram.

4.  **Shift ImGui Frame Management to `Application` calling `ImGuiManager`:**
    *   **Files:** `src/core/Application.cpp`, `src/core/ImGuiManager.hpp`, `src/core/ImGuiManager.cpp`
    *   **Task:**
        *   `ImGuiManager`: Ensure its `NewFrame()` method calls `ImGui_ImplSDLRenderer3_NewFrame()`, `ImGui_ImplSDL3_NewFrame()`, and `ImGui::NewFrame()`. Ensure its `Render()` method calls `ImGui::Render()` and then `ImGui_ImplSDLRenderer3_RenderDrawData()`.
        *   `Application::Render()`:
            *   Call `m_imguiManager->NewFrame()` at the beginning.
            *   Call `m_renderer->Clear()`.
            *   Call ImGui rendering logic (see next step).
            *   Call `m_imguiManager->Render()` (which gets draw data and submits to SDL_Renderer).
            *   Call `m_renderer->Present()`.
    *   **Rationale:** `ImGuiManager` abstracts ImGui backend specifics; `Application` orchestrates the frame.

5.  **Refactor UI Window Management in `Application`:**
    *   **Files:** `src/core/Application.hpp`, `src/core/Application.cpp`
    *   **Task:**
        *   `Application` should instantiate and own UI window objects (e.g., `PCBViewerWindow`, `SettingsWindow`, `PcbDetailsWindow`, a new `MainMenuBar` class).
        *   In `Application::Render()` (or an `UpdateUI()` method called from `Render()` or `Update()`):
            *   After `m_imguiManager->NewFrame()`, call the respective `RenderUI()` methods of these window/UI objects.
            *   Example: `m_mainMenuBar->RenderUI()`, `m_pcbViewerWindow->RenderUI(static_cast<SDL_Renderer*>(m_renderer->GetRendererHandle()))`, etc.
        *   Remove direct ImGui widget rendering from `Application::Update()` or `Application::Render()` itself, delegating it to these specific UI classes.
    *   **Rationale:** Adheres to the principle of UI components managing their own interactions.

6.  **Consolidate PCB View Interaction Logic:**
    *   **Files:** `src/core/Application.cpp`, `src/ui/windows/PCBViewerWindow.cpp`
    *   **Task:**
        *   Ensure any PCB view-specific interaction logic currently in `Application` (like mouse wheel zoom mentioned in notes) is handled by the `InteractionManager` within `PCBViewerWindow`.
        *   `PCBViewerWindow::RenderUI()` is responsible for passing ImGui IO state and viewport details to its `InteractionManager`.
    *   **Rationale:** Adheres to the principle of UI components managing their own interactions.

## Phase 2: Rendering Abstraction Cleanup & Refocusing for Blend2D

**Objective:** Streamline rendering classes and prepare for Blend2D integration by clarifying roles.

1.  **Remove Redundant `ImGuiRenderer`:**
    *   **Files:** `src/core/ImGuiRenderer.hpp`, `src/core/ImGuiRenderer.cpp`
    *   **Task:** Delete these files. Update any CMakeLists.txt or build scripts.
    *   **Rationale:** `ImGuiManager` fulfills all ImGui setup, lifecycle, and backend interaction roles.

2.  **Refocus `RenderContext` for Blend2D:**
    *   **Files:** `src/render/RenderContext.hpp`, `src/render/RenderContext.cpp`
    *   **Task:**
        *   Remove SDL_Renderer management (`m_renderer`, `SDL_CreateRenderer`, `SDL_DestroyRenderer`).
        *   Add members to manage `BLContext` and the primary off-screen `BLImage` for PCB rendering.
        *   Update `Initialize()` to set up the `BLImage` (with a default size, possibly configurable) and `BLContext` targeting this image.
        *   `BeginFrame()` could clear the `BLImage`. `EndFrame()` could be a no-op or finalize BLContext operations if any.
        *   Provide accessors like `BLContext& GetBlend2DContext()` and `BLImage& GetTargetImage()`.
    *   **Rationale:** Dedicates `RenderContext` to managing the Blend2D environment as per the architecture diagram.

3.  **Refocus `RenderManager` as `PcbRenderer` (Blend2D Orchestrator):**
    *   **Files:** `src/render/RenderManager.hpp`, `src/render/RenderManager.cpp` (Consider renaming to `PcbRenderer.hpp/.cpp` or `Blend2DRenderer.hpp/.cpp` for clarity).
    *   **Task:**
        *   This class will become the `BD_PcbRenderer` ("PCB Rendering Logic") component.
        *   It should own/manage the refocused (Blend2D) `RenderContext` and a `RenderPipeline` (for Blend2D commands).
        *   `Initialize()`: Initialize its `RenderContext` and `RenderPipeline`.
        *   `Render(const Board& board, const Camera& camera, const Viewport& viewport)`: Main method to orchestrate rendering the PCB (and grid) to the `BLImage` in its `RenderContext`.
            *   Call `m_renderContext->BeginFrame()` (to clear `BLImage`).
            *   Use `m_renderPipeline` to process and draw board elements and the grid onto the `BLContext`.
            *   Call `m_renderContext->EndFrame()`.
        *   Provide a method to get the rendered `BLImage` or its pixel data.
    *   **Rationale:** Centralizes the Blend2D rendering logic for the PCB content.

4.  **Adapt `RenderPipeline` for Blend2D Operations:**
    *   **Files:** `src/render/RenderPipeline.hpp`, `src/render/RenderPipeline.cpp`
    *   **Task:**
        *   The `Initialize(RenderContext& context)` method will take the Blend2D-focused `RenderContext`.
        *   `BeginPipeline` and `EndPipeline` will operate on the `BLContext` from `RenderContext`.
        *   Implement methods to add and process "renderable" items (PCB elements, grid) using Blend2D drawing commands via the `BLContext`. This might involve defining an `IRenderable` interface for Blend2D drawing or specific methods for each element type.
    *   **Rationale:** Makes the pipeline concrete for Blend2D drawing commands.

## Phase 3: Integrating Blend2D PCB Rendering

**Objective:** Connect the Blend2D rendering pipeline to the application and display its output.

1.  **`Application` Uses `RenderManager` (as `PcbRenderer`):**
    *   **Files:** `src/core/Application.hpp`, `src/core/Application.cpp`
    *   **Task:**
        *   `Application` should own an instance of the refocused `RenderManager` (e.g., `m_pcbRenderer`).
        *   Initialize `m_pcbRenderer` in `InitializeSubsystems()`.
        *   In `Application::Render()` (or a dedicated scene update step):
            *   Call `m_pcbRenderer->Render(currentBoard, m_camera, m_viewport)` to render the PCB to the `BLImage`.
    *   **Rationale:** `Application` delegates PCB-specific rendering to the dedicated manager.

2.  **Update `PCBViewerWindow` to Display `BLImage` via `SDL_Texture`:**
    *   **Files:** `src/ui/windows/PCBViewerWindow.hpp`, `src/ui/windows/PCBViewerWindow.cpp`
    *   **Task:**
        *   `PCBViewerWindow::UpdateAndRenderToTexture()`:
            *   No longer sets itself as SDL_Render target.
            *   Instead, it will get the rendered `BLImage` (pixel data, width, height, stride) from the `PcbRenderer` (via `Application` or direct reference if appropriate).
            *   It will then use its existing `m_renderTexture` (`SDL_Texture*`) and the main `SDL_Renderer*` (from `Application::GetRenderer()`) to:
                1.  Ensure `m_renderTexture` is the correct size.
                2.  Lock `m_renderTexture`.
                3.  Copy pixel data from `BLImage` to the locked `SDL_Texture`.
                4.  Unlock `m_renderTexture`.
        *   The `RenderUI` method will continue to display this `m_renderTexture` using `ImGui::Image`.
    *   **Rationale:** Implements the H_SDL3_Texture step in the diagram.

3.  **Adapt `Grid::Render()` for Blend2D:**
    *   **Files:** `src/view/Grid.hpp`, `src/view/Grid.cpp`
    *   **Task:**
        *   Change `Grid::Render()` to take a `BLContext& bl_ctx` (from the Blend2D-focused `RenderContext`) instead of an `SDL_Renderer*`.
        *   Re-implement drawing logic using Blend2D path and stroke/fill operations.
        *   The `PcbRenderer` / `RenderManager` will call this method during its `Render` pass, providing the correct `BLContext`.
    *   **Rationale:** Integrates grid rendering into the Blend2D pipeline for consistent appearance.

## Phase 4: Utilities & Final Polish

**Objective:** Address code duplication and other minor improvements.

1.  **Create Common String Utilities:**
    *   **Files:** New `src/utils/StringUtils.hpp`, `src/utils/StringUtils.cpp` (or similar).
    *   **Task:**
        *   Move `ReplaceAll` and `EscapeNewlines`/`UnescapeNewlines` functions from `Config.cpp` (and `main.cpp` if duplicated there) into this common utility.
        *   Update `Config.cpp` and other locations to use these utility functions.
    *   **Rationale:** DRY principle.

2.  **Review and Address Minor TODOs:**
    *   **Task:** Go through `index.md` and code comments for other smaller TODOs or minor refactoring cues (e.g., keybind conflict UI in `SettingsWindow`) and address them as time permits.
    *   **Rationale:** General code quality improvement.

This roadmap provides a structured approach. Each major number can be a primary task, and the sub-bullets are the detailed actions. It's important to test thoroughly after each significant step. 
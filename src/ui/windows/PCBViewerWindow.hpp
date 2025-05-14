#pragma once

#include <memory>
#include <string>
#include "imgui.h" // For ImVec2, ImGuiWindowFlags etc.

// Forward declarations
class Camera;
class Viewport;
class Grid;
class GridSettings;
class InteractionManager;
struct SDL_Renderer;
struct SDL_Texture; // For rendering the PCB view onto a texture
// class InteractionManager; // Will be needed for pan/zoom etc.

class PCBViewerWindow {
public:
    PCBViewerWindow(
        std::shared_ptr<Camera> camera,
        std::shared_ptr<Viewport> viewport,
        std::shared_ptr<Grid> grid,
        std::shared_ptr<GridSettings> gridSettings
    );
    ~PCBViewerWindow();

    PCBViewerWindow(const PCBViewerWindow&) = delete;
    PCBViewerWindow& operator=(const PCBViewerWindow&) = delete;
    PCBViewerWindow(PCBViewerWindow&&) = delete;
    PCBViewerWindow& operator=(PCBViewerWindow&&) = delete;

    // Renders the ImGui window and manages the content within.
    // renderer is needed to create/update the SDL_Texture.
    void RenderUI(SDL_Renderer* renderer);

    bool IsWindowFocused() const { return m_isFocused; }
    bool IsWindowHovered() const { return m_isHovered; }
    bool IsWindowVisible() const { return m_isOpen; }
    void SetVisible(bool visible) { m_isOpen = visible; }

private:
    std::string m_windowName = "PCB View";
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<Viewport> m_viewport; // This viewport will be updated by the ImGui window size
    std::shared_ptr<Grid> m_grid;
    std::shared_ptr<GridSettings> m_gridSettings;
    std::unique_ptr<InteractionManager> m_interactionManager;

    SDL_Texture* m_renderTexture = nullptr;
    int m_textureWidth = 0;
    int m_textureHeight = 0;

    bool m_isOpen = true; // Controls ImGui::Begin p_open argument
    bool m_isFocused = false;
    bool m_isHovered = false;
    bool m_isContentRegionHovered = false; // Specifically if mouse is over the texture/render area

    ImVec2 m_contentRegionTopLeftScreen; // Renamed for clarity (screen coordinates)
    ImVec2 m_contentRegionSize;     // Size of the renderable content area

    void InitializeTexture(SDL_Renderer* renderer, int width, int height);
    void ReleaseTexture();

    void UpdateAndRenderToTexture(SDL_Renderer* renderer); // Orchestrates rendering onto m_renderTexture
}; 
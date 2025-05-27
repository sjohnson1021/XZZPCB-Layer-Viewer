#pragma once

#include <memory>
#include <string>
#include "imgui.h" // For ImVec2, ImGuiWindowFlags etc.
#include "core/ControlSettings.hpp"
#include "core/BoardDataManager.hpp"
#include <functional>
// #include <blend2d.h> // Included in .cpp, forward declare if only types used here

// Forward declarations
class Camera;
class Viewport;
class Grid;
class GridSettings;
class InteractionManager;
class ControlSettings;
class Board;       // Added for OnBoardLoaded
class PcbRenderer; // Forward declare PcbRenderer
struct SDL_Renderer;
struct SDL_Texture; // For rendering the PCB view onto a texture
// class InteractionManager; // Will be needed for pan/zoom etc.

class PCBViewerWindow
{
public:
    PCBViewerWindow(
        std::shared_ptr<Camera> camera,
        std::shared_ptr<Viewport> viewport,
        std::shared_ptr<Grid> grid,
        std::shared_ptr<GridSettings> gridSettings,
        std::shared_ptr<ControlSettings> controlSettings,
        std::shared_ptr<BoardDataManager> boardDataManager);
    ~PCBViewerWindow();

    PCBViewerWindow(const PCBViewerWindow &) = delete;
    PCBViewerWindow &operator=(const PCBViewerWindow &) = delete;
    PCBViewerWindow(PCBViewerWindow &&) = delete;
    PCBViewerWindow &operator=(PCBViewerWindow &&) = delete;

    // // void RenderUI(SDL_Renderer* renderer, PcbRenderer* pcbRenderer); // OLD METHOD

    // Renders the ImGui window. Integrates a callback for PcbRenderer to render its content
    // at the correct time (after viewport sizing, before texture update and ImGui::Image).
    void RenderIntegrated(SDL_Renderer *sdlRenderer, PcbRenderer *pcbRenderer, const std::function<void()> &pcbRenderCallback);

    void OnBoardLoaded(const std::shared_ptr<Board> &board, PcbRenderer *pcbRenderer); // Added PcbRenderer

    bool IsWindowFocused() const { return m_isFocused; }
    bool IsWindowHovered() const { return m_isHovered; }
    bool IsWindowVisible() const { return m_isOpen; }
    void SetVisible(bool visible) { m_isOpen = visible; }

private:
    // This method will handle getting data from PcbRenderer and updating m_renderTexture
    void UpdateTextureFromPcbRenderer(SDL_Renderer *sdlRenderer, PcbRenderer *pcbRenderer);

    std::string m_windowName = "PCB View";
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<Viewport> m_viewport; // This viewport will be updated by the ImGui window size
    std::shared_ptr<Grid> m_grid;
    std::shared_ptr<GridSettings> m_gridSettings;
    std::unique_ptr<InteractionManager> m_interactionManager;
    std::shared_ptr<ControlSettings> m_controlSettings;
    std::shared_ptr<BoardDataManager> m_boardDataManager;

    SDL_Texture *m_renderTexture = nullptr;
    int m_textureWidth = 0;
    int m_textureHeight = 0;

    bool m_isOpen = true; // Controls ImGui::Begin p_open argument
    bool m_isFocused = false;
    bool m_isHovered = false;
    bool m_isContentRegionHovered = false; // Specifically if mouse is over the texture/render area

    ImVec2 m_contentRegionTopLeftScreen; // Renamed for clarity (screen coordinates)
    ImVec2 m_contentRegionSize;          // Size of the renderable content area

    // For delayed texture resizing
    ImVec2 m_desiredTextureSize = {0, 0};
    int m_resizeCooldownFrames = 0;
    static const int RESIZE_COOLDOWN_MAX = 5; // Frames to wait

    void InitializeTexture(SDL_Renderer *renderer, int width, int height);
    void ReleaseTexture();

    // void UpdateAndRenderToTexture(SDL_Renderer* renderer); // This role is now split/handled differently
    // The old UpdateAndRenderToTexture was for when PCBViewerWindow *drew* to the texture itself.
    // Now it *copies* from PcbRenderer's image to the texture.
};
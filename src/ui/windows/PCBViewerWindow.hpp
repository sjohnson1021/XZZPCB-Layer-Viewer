#pragma once

#include <functional>
#include <memory>
#include <string>

#include "imgui.h"  // For ImVec2, ImGuiWindowFlags etc.

#include "core/BoardDataManager.hpp"
#include "core/ControlSettings.hpp"
// #include <blend2d.h> // Included in .cpp, forward declare if only types used here

// Forward declarations
class Camera;
class Viewport;
class Grid;
class GridSettings;
class InteractionManager;
class ControlSettings;
class Board;        // Added for OnBoardLoaded
class PcbRenderer;  // Forward declare PcbRenderer
struct SDL_Renderer;
struct SDL_Texture;  // For rendering the PCB view onto a texture
// class InteractionManager; // Will be needed for pan/zoom etc.

class PCBViewerWindow
{
public:
    PCBViewerWindow(std::shared_ptr<Camera> camera,
                    std::shared_ptr<Viewport> viewport,
                    std::shared_ptr<Grid> grid,
                    std::shared_ptr<GridSettings> grid_settings,
                    std::shared_ptr<ControlSettings> control_settings,
                    std::shared_ptr<BoardDataManager> board_data_manager);
    ~PCBViewerWindow();

    PCBViewerWindow(const PCBViewerWindow&) = delete;
    PCBViewerWindow& operator=(const PCBViewerWindow&) = delete;
    PCBViewerWindow(PCBViewerWindow&&) = delete;
    PCBViewerWindow& operator=(PCBViewerWindow&&) = delete;

    // // void RenderUI(SDL_Renderer* renderer, PcbRenderer* pcbRenderer); // OLD METHOD

    // Renders the ImGui window. Integrates a callback for PcbRenderer to render its content
    // at the correct time (after viewport sizing, before texture update and ImGui::Image).
    void RenderIntegrated(SDL_Renderer* sdl_renderer, PcbRenderer* pcb_renderer, const std::function<void()>& pcb_render_callback);

    void OnBoardLoaded(const std::shared_ptr<Board>& board, PcbRenderer* pcb_renderer);  // Added PcbRenderer

    [[nodiscard]] bool IsWindowFocused() const { return m_is_focused_; }
    [[nodiscard]] bool IsWindowHovered() const { return m_is_hovered_; }
    [[nodiscard]] bool IsWindowVisible() const { return m_is_open_; }
    void SetVisible(bool visible) { m_is_open_ = visible; }

private:
    // This method will handle getting data from PcbRenderer and updating m_renderTexture
    void UpdateTextureFromPcbRenderer(SDL_Renderer* sdl_renderer, PcbRenderer* pcb_renderer);

    // Render grid measurement overlay within the PCB viewer window
    void RenderGridMeasurementOverlay();

    std::string m_window_name_ = "PCB Viewer";
    std::shared_ptr<Camera> m_camera_;
    std::shared_ptr<Viewport> m_viewport_;  // This viewport will be updated by the ImGui window size
    std::shared_ptr<Grid> m_grid_;
    std::shared_ptr<GridSettings> m_grid_settings_;
    std::unique_ptr<InteractionManager> m_interaction_manager_;
    std::shared_ptr<ControlSettings> m_control_settings_;
    std::shared_ptr<BoardDataManager> m_board_data_manager_;

    SDL_Texture* m_render_texture_ = nullptr;
    int m_texture_width_ = 0;
    int m_texture_height_ = 0;

    bool m_is_open_ = true;  // Controls ImGui::Begin p_open argument
    bool m_is_focused_ = false;
    bool m_is_hovered_ = false;
    bool m_is_content_region_hovered_ = false;  // Specifically if mouse is over the texture/render area

    ImVec2 m_content_region_top_left_screen_;  // Renamed for clarity (screen coordinates)
    ImVec2 m_content_region_size_;             // Size of the renderable content area

    // For delayed texture resizing
    ImVec2 m_desired_texture_size_ = {0, 0};
    int m_resize_cooldown_frames_ = 0;
    static const int kResizeCooldownMax = 5;  // Frames to wait

    void InitializeTexture(SDL_Renderer* renderer, int width, int height);
    void ReleaseTexture();

    // void UpdateAndRenderToTexture(SDL_Renderer* renderer); // This role is now split/handled differently
    // The old UpdateAndRenderToTexture was for when PCBViewerWindow *drew* to the texture itself.
    // Now it *copies* from PcbRenderer's image to the texture.
};

#include "ui/windows/PCBViewerWindow.hpp"

#include <algorithm>  // For std::max
#include <cmath>      // For std::round
#include <cstddef>
#include <iostream>  // For logging in OnBoardLoaded

#include <SDL3/SDL.h>
#include <SDL3/SDL_blendmode.h>
#include <blend2d.h>

#include "core/BoardDataManager.hpp"
#include "core/ControlSettings.hpp"
#include "pcb/Board.hpp"
#include "render/PcbRenderer.hpp"
#include "ui/interaction/InteractionManager.hpp"
#include "view/Camera.hpp"
#include "view/Grid.hpp"
#include "view/GridSettings.hpp"
#include "view/Viewport.hpp"

// Constructor
PCBViewerWindow::PCBViewerWindow(std::shared_ptr<Camera> camera,
                                 std::shared_ptr<Viewport> viewport,
                                 std::shared_ptr<Grid> grid,
                                 std::shared_ptr<GridSettings> grid_settings,
                                 std::shared_ptr<ControlSettings> control_settings,
                                 std::shared_ptr<BoardDataManager> board_data_manager)
    : m_camera_(camera),
      m_viewport_(viewport),
      m_grid_(grid),
      m_grid_settings_(grid_settings),
      m_control_settings_(control_settings),
      m_board_data_manager_(board_data_manager),
      m_is_open_(true),
      m_is_focused_(false),
      m_is_hovered_(false),
      m_is_content_region_hovered_(false),
      m_render_texture_(nullptr),
      m_texture_width_(100),
      m_texture_height_(100),
      m_content_region_top_left_screen_(ImVec2(0.0f, 0.0f)),
      m_content_region_size_(ImVec2(100.0f, 100.0f)),
      m_desired_texture_size_(ImVec2(0.0f, 0.0f)),
      m_resize_cooldown_frames_(-1)
{
    m_interaction_manager_ = std::make_unique<InteractionManager>(m_camera_, m_viewport_, m_control_settings_, m_board_data_manager_);
}

// Destructor
PCBViewerWindow::~PCBViewerWindow()
{
    ReleaseTexture();
}

// Initialize or resize the SDL_Texture for displaying Blend2D output
void PCBViewerWindow::InitializeTexture(SDL_Renderer* renderer, int width, int height)
{
    // Release any existing texture first
    if (m_render_texture_) {
        SDL_DestroyTexture(m_render_texture_);
        m_render_texture_ = nullptr;
    }
    m_texture_width_ = std::max(1, width);
    m_texture_height_ = std::max(1, height);

    // Attempt to create texture with ARGB8888 format first
    m_render_texture_ = SDL_CreateTexture(renderer,
                                          SDL_PIXELFORMAT_ARGB8888,  // This matches Blend2D's BL_FORMAT_PRGB32
                                          SDL_TEXTUREACCESS_STREAMING,
                                          m_texture_width_,
                                          m_texture_height_);

    if (!m_render_texture_) {
        SDL_Log("PCBViewerWindow: Failed to create texture with ARGB8888. Trying RGBA8888...");

        // Try RGBA8888
        m_render_texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, m_texture_width_, m_texture_height_);

        if (!m_render_texture_) {
            SDL_Log("PCBViewerWindow: RGBA8888 attempt failed. Trying ABGR8888...");

            // Try ABGR8888 as a last resort
            m_render_texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, m_texture_width_, m_texture_height_);

            if (!m_render_texture_) {
                SDL_Log("PCBViewerWindow: All texture format attempts failed: %s", SDL_GetError());
                return;
            }
        }
    }

    // Set blend mode for transparency support
    if (!SDL_SetTextureBlendMode(m_render_texture_, SDL_BLENDMODE_BLEND_PREMULTIPLIED)) {
        SDL_Log("PCBViewerWindow: Failed to set blend mode: %s", SDL_GetError());
    }
}

void PCBViewerWindow::ReleaseTexture()
{
    if (m_render_texture_) {
        SDL_DestroyTexture(m_render_texture_);
        m_render_texture_ = nullptr;
        m_texture_width_ = 0;
        m_texture_height_ = 0;
    }
}

// New method to update our SDL_Texture from PcbRenderer's BLImage
void PCBViewerWindow::UpdateTextureFromPcbRenderer(SDL_Renderer* sdl_renderer, PcbRenderer* pcb_renderer)
{
    if (!pcb_renderer || !sdl_renderer)
        return;

    const BLImage& bl_image = pcb_renderer->GetRenderedImage();
    if (bl_image.empty()) {
        SDL_Log("PCBViewerWindow: BLImage from PcbRenderer is empty.");
        return;
    }

    int bl_image_width = bl_image.width();
    int bl_image_height = bl_image.height();

    // Get the BLImage data
    BLImageData bl_image_data;
    bl_image.getData(&bl_image_data);

    // Check for null pixel data
    if (!bl_image_data.pixelData) {
        SDL_Log("PCBViewerWindow::UpdateTextureFromPcbRenderer: BLImage pixel data is null");
        return;
    }

    // Check if texture needs to be recreated (doesn't exist or size changed)
    bool create_new_texture = false;
    if (!m_render_texture_) {
        create_new_texture = true;
    } else {
        float current_width, current_height;
        if (SDL_GetTextureSize(m_render_texture_, &current_width, &current_height) == 0) {
            if (static_cast<int>(current_width) != bl_image_width || static_cast<int>(current_height) != bl_image_height) {
                create_new_texture = true;
            }
        } else {
            // GetTextureSize failed, recreate to be safe
            create_new_texture = true;
        }
    }

    // Create new texture if needed
    if (create_new_texture) {
        // Release any old texture
        if (m_render_texture_) {
            SDL_DestroyTexture(m_render_texture_);
            m_render_texture_ = nullptr;
        }

        // Create a new texture with the correct dimensions
        m_render_texture_ = SDL_CreateTexture(sdl_renderer,
                                              SDL_PIXELFORMAT_ARGB8888,  // Matches Blend2D's BL_FORMAT_PRGB32
                                              SDL_TEXTUREACCESS_STREAMING,
                                              bl_image_width,
                                              bl_image_height);

        if (!m_render_texture_) {
            SDL_Log("PCBViewerWindow: Failed to create texture: %s", SDL_GetError());
            return;
        }

        // Set blend mode for transparency - use premultiplied for Blend2D compatibility
        if (!SDL_SetTextureBlendMode(m_render_texture_, SDL_BLENDMODE_BLEND_PREMULTIPLIED)) {
            SDL_Log("PCBViewerWindow: Failed to set premultiplied blend mode: %s", SDL_GetError());
        }

        m_texture_width_ = bl_image_width;
        m_texture_height_ = bl_image_height;
    }

    // Update the texture data directly from Blend2D's buffer
    if (!SDL_UpdateTexture(m_render_texture_, NULL, bl_image_data.pixelData, bl_image_data.stride)) {
        SDL_Log("PCBViewerWindow: Failed to update texture: %s", SDL_GetError());
    }
}

// Main ImGui rendering function for this window, integrating PcbRenderer callback
void PCBViewerWindow::RenderIntegrated(SDL_Renderer* sdl_renderer, PcbRenderer* pcb_renderer, const std::function<void()>& pcb_render_callback)
{
    if (!m_is_open_ && m_render_texture_) {
        ReleaseTexture();
        m_resize_cooldown_frames_ = -1;
        m_desired_texture_size_ = ImVec2(0, 0);
    }

    if (!m_is_open_) {
        // Reset focus/hover states if the window is not even supposed to be open
        m_is_focused_ = false;
        m_is_hovered_ = false;
        m_is_content_region_hovered_ = false;
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    bool window_open_this_frame = ImGui::Begin(m_window_name_.c_str(), &m_is_open_, window_flags);

    if (window_open_this_frame) {
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));  // Ensure cursor is at the top-left of the content area

        m_is_focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_is_hovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        m_content_region_size_ = ImGui::GetContentRegionAvail();
        m_content_region_top_left_screen_ = ImGui::GetCursorScreenPos();
        bool new_size_is_valid = (m_content_region_size_.x > 0 && m_content_region_size_.y > 0);

        // Update the shared viewport object
        if (m_viewport_) {
            m_viewport_->SetDimensions(0, 0, static_cast<int>(std::round(m_content_region_size_.x)), static_cast<int>(std::round(m_content_region_size_.y)));
            // Inform PcbRenderer if its target image size needs to change.
            if (pcb_renderer && new_size_is_valid) {
                const BLImage& current_pcb_image = pcb_renderer->GetRenderedImage();
                int roundedWidth = static_cast<int>(std::round(m_content_region_size_.x));
                int roundedHeight = static_cast<int>(std::round(m_content_region_size_.y));

                if (current_pcb_image.width() != roundedWidth || current_pcb_image.height() != roundedHeight) {
                    pcb_renderer->OnViewportResized(roundedWidth, roundedHeight);
                }
            }
        }

        // Execute the PcbRenderer::Render() call via the callback
        if (pcb_render_callback) {
            pcb_render_callback();
        }

        // Update our SDL_Texture from PcbRenderer's BLImage (which should now be current)
        // Only update texture if the PcbRenderer actually drew something new.
        if (pcb_renderer && pcb_renderer->WasFrameJustRendered()) {
            UpdateTextureFromPcbRenderer(sdl_renderer, pcb_renderer);
        }

        if (m_render_texture_) {
            ImGui::Image(reinterpret_cast<ImTextureID>(m_render_texture_), ImVec2(m_texture_width_, m_texture_height_));

            // Handle interaction after the image is drawn, so ImGui::IsItemHovered() refers to the image
            if (m_interaction_manager_ && (m_is_focused_ || m_is_hovered_)) {
                m_is_content_region_hovered_ = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);
                m_interaction_manager_->ProcessInput(ImGui::GetIO(), m_is_focused_, m_is_content_region_hovered_, m_content_region_top_left_screen_, m_content_region_size_, pcb_renderer);
            } else {
                m_is_content_region_hovered_ = false;
            }
        } else {
            ImGui::Text("PcbRenderer output not available or texture creation failed. Desired: (%.0f, %.0f)", m_content_region_size_.x, m_content_region_size_.y);
        }
    } else {
        // ImGui::Begin returned false (e.g. window collapsed, or m_isOpen was set to false by ImGui::Begin itself)
        m_is_focused_ = false;
        m_is_hovered_ = false;
        m_is_content_region_hovered_ = false;
        if (m_render_texture_) {  // If window is effectively closed by ImGui, release texture
            ReleaseTexture();
        }
    }

    // This PopStyleVar and End must be called if ImGui::Begin was called,
    // regardless of whether it returned true or false, to keep ImGui state balanced.
    ImGui::End();
    ImGui::PopStyleVar();
}

void PCBViewerWindow::OnBoardLoaded(const std::shared_ptr<Board>& board, PcbRenderer* pcb_renderer)
{
    if (board) {
        std::cout << "PCBViewerWindow: New board loaded: " << board->GetFilePath() << std::endl;
    } else {
        std::cout << "PCBViewerWindow: Board unloaded (or load failed)." << std::endl;
    }

    if (pcb_renderer) {
        pcb_renderer->MarkBoardDirty();
        pcb_renderer->MarkGridDirty();  // Also mark grid dirty as board extents might change grid appearance
    }
}

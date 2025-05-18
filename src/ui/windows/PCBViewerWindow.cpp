#include "ui/windows/PCBViewerWindow.hpp"
#include "pcb/Board.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "view/Grid.hpp"
#include "view/GridSettings.hpp"
#include "ui/interaction/InteractionManager.hpp"
#include "core/ControlSettings.hpp"
#include <SDL3/SDL.h>
#include <algorithm> // For std::max
#include <iostream> // For logging in OnBoardLoaded

// Constructor
PCBViewerWindow::PCBViewerWindow(
    std::shared_ptr<Camera> camera,
    std::shared_ptr<Viewport> viewport,
    std::shared_ptr<Grid> grid,
    std::shared_ptr<GridSettings> gridSettings,
    std::shared_ptr<ControlSettings> controlSettings)
    : m_camera(camera)
    , m_viewport(viewport) // This viewport represents the render texture area
    , m_grid(grid)
    , m_gridSettings(gridSettings)
    , m_controlSettings(controlSettings)
    , m_isFocused(false)
    , m_isHovered(false)
    , m_renderTexture(nullptr)
    , m_textureWidth(100) // Initial placeholder size
    , m_textureHeight(100) {
    m_interactionManager = std::make_unique<InteractionManager>(m_camera, m_viewport, m_controlSettings);
}

// Destructor
PCBViewerWindow::~PCBViewerWindow() {
    ReleaseTexture();
}

// Initialize or resize the SDL_Texture
void PCBViewerWindow::InitializeTexture(SDL_Renderer* renderer, int width, int height) {
    if (m_renderTexture) {
        SDL_DestroyTexture(m_renderTexture);
        m_renderTexture = nullptr;
    }
    m_textureWidth = std::max(1, width);   // Ensure positive dimensions
    m_textureHeight = std::max(1, height);

    m_renderTexture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_RGBA8888,
                                      SDL_TEXTUREACCESS_TARGET,
                                      m_textureWidth,
                                      m_textureHeight);

    if (!m_renderTexture) {
        SDL_Log("Failed to create render texture: %s", SDL_GetError());
        // Handle error, perhaps throw or set a flag
    } else { // Ensure blend mode is set only if texture creation succeeded
        SDL_SetTextureBlendMode(m_renderTexture, SDL_BLENDMODE_BLEND);
    }
}

// Release the SDL_Texture
void PCBViewerWindow::ReleaseTexture() {
    if (m_renderTexture) {
        SDL_DestroyTexture(m_renderTexture);
        m_renderTexture = nullptr;
        m_textureWidth = 0;
        m_textureHeight = 0;
    }
}

// Render the scene (grid, etc.) onto the internal SDL_Texture
void PCBViewerWindow::UpdateAndRenderToTexture(SDL_Renderer* renderer) {
    if (!m_renderTexture || !renderer || !m_camera || !m_grid || !m_viewport) {
        return;
    }

    SDL_SetRenderTarget(renderer, m_renderTexture);
    
    // Set a background color for the texture (e.g., dark gray)
    // This color should ideally come from theme/settings eventually
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    // Update the viewport associated with this window/texture
    // The viewport for rendering to texture always starts at (0,0)
    m_viewport->SetDimensions(0, 0, m_textureWidth, m_textureHeight);

    if (m_grid) {
        m_grid->Render(renderer, *m_camera, *m_viewport);
    }

    // Render other PCB elements here in the future

    SDL_SetRenderTarget(renderer, nullptr); // Reset render target to default (screen)
}

// Main ImGui rendering function for this window
void PCBViewerWindow::RenderUI(SDL_Renderer* renderer) {
    if (!m_isOpen && m_renderTexture) {
        ReleaseTexture(); 
        m_resizeCooldownFrames = -1; 
        m_desiredTextureSize = ImVec2(0,0); 
    }

    if (!m_isOpen) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; // RESTORED FLAGS

    bool window_open = ImGui::Begin(m_windowName.c_str(), &m_isOpen, window_flags); // USE RESTORED FLAGS
    ImGui::PopStyleVar(); // Always pop style var right after Begin

    if (window_open) {
        m_isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        ImVec2 newContentRegionSize = ImGui::GetContentRegionAvail();
        bool newSizeIsValid = (newContentRegionSize.x > 0 && newContentRegionSize.y > 0);

        m_contentRegionSize = newContentRegionSize;
        ImVec2 contentStart = ImGui::GetCursorScreenPos(); 

        m_viewport->SetDimensions(static_cast<int>(contentStart.x), static_cast<int>(contentStart.y), static_cast<int>(m_contentRegionSize.x), static_cast<int>(m_contentRegionSize.y));

        if (!newSizeIsValid) {
        } else { 
            bool currentTextureSizeDiffersFromContentRegion = (!m_renderTexture ||
                                                               m_textureWidth != static_cast<int>(newContentRegionSize.x) ||
                                                               m_textureHeight != static_cast<int>(newContentRegionSize.y));

            if (currentTextureSizeDiffersFromContentRegion) {
                if (m_desiredTextureSize.x != newContentRegionSize.x || m_desiredTextureSize.y != newContentRegionSize.y) {
                    m_desiredTextureSize = newContentRegionSize;
                    m_resizeCooldownFrames = RESIZE_COOLDOWN_MAX; 
                } else if (m_resizeCooldownFrames < 0) { 
                    m_resizeCooldownFrames = RESIZE_COOLDOWN_MAX; 
                }
            }
        }

        if (m_resizeCooldownFrames > 0) {
            m_resizeCooldownFrames--;
        }

        if (m_resizeCooldownFrames == 0) {
            if (m_desiredTextureSize.x > 0 && m_desiredTextureSize.y > 0 &&
                (!m_renderTexture || 
                 m_textureWidth != static_cast<int>(m_desiredTextureSize.x) ||
                 m_textureHeight != static_cast<int>(m_desiredTextureSize.y))) {
                InitializeTexture(renderer, static_cast<int>(m_desiredTextureSize.x), static_cast<int>(m_desiredTextureSize.y));
            }
            m_resizeCooldownFrames = -1; 
        }

        if (m_renderTexture) {
            UpdateAndRenderToTexture(renderer);
            ImGui::Image(reinterpret_cast<ImTextureID>(m_renderTexture), m_contentRegionSize);
        } else {
            ImGui::Text("Texture not available or creation failed. ContentRegion: (%.1f, %.1f)", m_contentRegionSize.x, m_contentRegionSize.y);
            if (newSizeIsValid && m_resizeCooldownFrames < 0 && (m_desiredTextureSize.x != newContentRegionSize.x || m_desiredTextureSize.y != newContentRegionSize.y)) { 
                m_desiredTextureSize = newContentRegionSize;
                m_resizeCooldownFrames = RESIZE_COOLDOWN_MAX;
            }
        }

        if (m_interactionManager && (m_isFocused || m_isHovered)) {
            m_isContentRegionHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly); 
            m_interactionManager->ProcessInput(ImGui::GetIO(), m_isFocused, m_isContentRegionHovered, contentStart, m_contentRegionSize);
        } else {
            m_isContentRegionHovered = false;
        }
    } else {
        m_isFocused = false;
        m_isHovered = false;
        m_isContentRegionHovered = false;
    }
    
    ImGui::End(); // FIXED: Always call End() to match the Begin() call
}

void PCBViewerWindow::OnBoardLoaded(const std::shared_ptr<Board>& board) {
    // For now, just log that a new board has been loaded.
    // In the future, this could reset camera, update zoom to fit board, etc.
    if (board) {
        std::cout << "PCBViewerWindow: New board loaded: " << board->GetFilePath() << std::endl;
    }
    else {
        std::cout << "PCBViewerWindow: Board unloaded (or load failed)." << std::endl;
    }
    // Example: Reset camera or zoom to fit new board
    // m_camera->ResetView(); 
    // m_camera->ZoomToFit(board->GetBoundingBox());
} 
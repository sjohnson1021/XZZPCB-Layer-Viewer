#include "ui/windows/PCBViewerWindow.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "view/Grid.hpp"
#include "view/GridSettings.hpp"
#include "ui/interaction/InteractionManager.hpp"
#include <SDL3/SDL.h>
#include <algorithm> // For std::max

// Constructor
PCBViewerWindow::PCBViewerWindow(
    std::shared_ptr<Camera> camera,
    std::shared_ptr<Viewport> viewport,
    std::shared_ptr<Grid> grid,
    std::shared_ptr<GridSettings> gridSettings)
    : m_camera(camera)
    , m_viewport(viewport) // This viewport represents the render texture area
    , m_grid(grid)
    , m_gridSettings(gridSettings) {
    m_interactionManager = std::make_unique<InteractionManager>(m_camera, m_viewport);
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
    }
    SDL_SetTextureBlendMode(m_renderTexture, SDL_BLENDMODE_BLEND);
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
    if (!m_isOpen) {
        ReleaseTexture(); // Release texture if window is closed to free VRAM
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // No padding for the render area
    if (ImGui::Begin(m_windowName.c_str(), &m_isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        m_isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        ImVec2 contentMinPos = ImGui::GetWindowContentRegionMin();
        ImVec2 contentMaxPos = ImGui::GetWindowContentRegionMax();
        m_contentRegionSize = {contentMaxPos.x - contentMinPos.x, contentMaxPos.y - contentMinPos.y};
        
        // Get the absolute screen position of the top-left of the content region
        m_contentRegionTopLeftScreen = ImGui::GetWindowPos();
        m_contentRegionTopLeftScreen.x += contentMinPos.x;
        m_contentRegionTopLeftScreen.y += contentMinPos.y;

        // Ensure texture matches content area size
        if (!m_renderTexture || (int)m_contentRegionSize.x != m_textureWidth || (int)m_contentRegionSize.y != m_textureHeight) {
            if (m_contentRegionSize.x > 0 && m_contentRegionSize.y > 0) {
                InitializeTexture(renderer, (int)m_contentRegionSize.x, (int)m_contentRegionSize.y);
            }
        }

        if (m_renderTexture) {
            UpdateAndRenderToTexture(renderer);
            // Display the texture. Y is flipped because ImGui expects top-left UV (0,0) and bottom-right UV (1,1)
            // SDL_Textures are typically rendered with (0,0) at top-left as well.
            ImGui::Image(reinterpret_cast<ImTextureID>(m_renderTexture), m_contentRegionSize);

            // Check if the image (content region) is hovered for more precise input handling
            m_isContentRegionHovered = ImGui::IsItemHovered();
        } else {
            ImGui::Text("Error: Render texture not available.");
            m_isContentRegionHovered = false;
        }

        // Delegate input processing to InteractionManager
        if (m_interactionManager) {
            ImGuiIO& io = ImGui::GetIO();
            // Pass the screen-space top-left of the content region for mouse coordinate mapping
            m_interactionManager->ProcessInput(io, m_isFocused, m_isContentRegionHovered, m_contentRegionTopLeftScreen, m_contentRegionSize);
        }

    } else {
        // Window is not visible (e.g. collapsed or culled by ImGui), but Begin still runs the lambda if it's not closed
        // If Begin returns false because m_isOpen was set to false (user clicked close button), this block is skipped
        // and m_isOpen is false for next frame.
        m_isFocused = false;
        m_isHovered = false;
        m_isContentRegionHovered = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();

    if (!m_isOpen) { // If Begin set m_isOpen to false (window was closed)
        ReleaseTexture();
    }
} 
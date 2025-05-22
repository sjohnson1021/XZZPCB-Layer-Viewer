#include "ui/windows/PCBViewerWindow.hpp"
#include "pcb/Board.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "view/Grid.hpp"
#include "view/GridSettings.hpp"
#include "ui/interaction/InteractionManager.hpp"
#include "core/ControlSettings.hpp"
#include "render/PcbRenderer.hpp"
#include "core/BoardDataManager.hpp"
#include <SDL3/SDL.h>
#include <blend2d.h>
#include <algorithm> // For std::max
#include <iostream> // For logging in OnBoardLoaded
#include <cmath> // For std::round

// Constructor
PCBViewerWindow::PCBViewerWindow(
    std::shared_ptr<Camera> camera,
    std::shared_ptr<Viewport> viewport,
    std::shared_ptr<Grid> grid,
    std::shared_ptr<GridSettings> gridSettings,
    std::shared_ptr<ControlSettings> controlSettings,
    std::shared_ptr<BoardDataManager> boardDataManager)
    : m_camera(camera)
    , m_viewport(viewport)
    , m_grid(grid)
    , m_gridSettings(gridSettings)
    , m_controlSettings(controlSettings)
    , m_boardDataManager(boardDataManager)
    , m_isOpen(true)
    , m_isFocused(false)
    , m_isHovered(false)
    , m_isContentRegionHovered(false)
    , m_renderTexture(nullptr)
    , m_textureWidth(100)
    , m_textureHeight(100)
    , m_contentRegionTopLeftScreen(ImVec2(0.0f, 0.0f))
    , m_contentRegionSize(ImVec2(100.0f, 100.0f))
    , m_desiredTextureSize(ImVec2(0.0f, 0.0f))
    , m_resizeCooldownFrames(-1) {
    m_interactionManager = std::make_unique<InteractionManager>(m_camera, m_viewport, m_controlSettings);
}

// Destructor
PCBViewerWindow::~PCBViewerWindow() {
    ReleaseTexture();
}

// Initialize or resize the SDL_Texture for displaying Blend2D output
void PCBViewerWindow::InitializeTexture(SDL_Renderer* renderer, int width, int height) {
    // Release any existing texture first
    if (m_renderTexture) {
        SDL_DestroyTexture(m_renderTexture);
        m_renderTexture = nullptr;
    }
    m_textureWidth = std::max(1, width);   
    m_textureHeight = std::max(1, height);

    // Attempt to create texture with ARGB8888 format first
    m_renderTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,  // This matches Blend2D's BL_FORMAT_PRGB32
        SDL_TEXTUREACCESS_STREAMING,
        m_textureWidth,
        m_textureHeight
    );

    if (!m_renderTexture) {
        SDL_Log("PCBViewerWindow: Failed to create texture with ARGB8888. Trying RGBA8888...");
        
        // Try RGBA8888
        m_renderTexture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            m_textureWidth,
            m_textureHeight
        );
        
        if (!m_renderTexture) {
            SDL_Log("PCBViewerWindow: RGBA8888 attempt failed. Trying ABGR8888...");
            
            // Try ABGR8888 as a last resort
            m_renderTexture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_ABGR8888,
                SDL_TEXTUREACCESS_STREAMING,
                m_textureWidth,
                m_textureHeight
            );
            
            if (!m_renderTexture) {
                SDL_Log("PCBViewerWindow: All texture format attempts failed: %s", SDL_GetError());
                return;
            }
        }
    }
    
    // Set blend mode for transparency support
    if (!SDL_SetTextureBlendMode(m_renderTexture, SDL_BLENDMODE_BLEND_PREMULTIPLIED)) {
        SDL_Log("PCBViewerWindow: Failed to set blend mode: %s", SDL_GetError());
    }
}

void PCBViewerWindow::ReleaseTexture() {
    if (m_renderTexture) {
        SDL_DestroyTexture(m_renderTexture);
        m_renderTexture = nullptr;
        m_textureWidth = 0;
        m_textureHeight = 0;
    }
}

// New method to update our SDL_Texture from PcbRenderer's BLImage
void PCBViewerWindow::UpdateTextureFromPcbRenderer(SDL_Renderer* sdlRenderer, PcbRenderer* pcbRenderer) {
    if (!pcbRenderer || !sdlRenderer) return;

    const BLImage& blImage = pcbRenderer->GetRenderedImage();
    if (blImage.empty()) {
        SDL_Log("PCBViewerWindow: BLImage from PcbRenderer is empty.");
        return;
    }

    int blImageWidth = blImage.width();
    int blImageHeight = blImage.height();
    
    // Get the BLImage data
    BLImageData blImageData;
    blImage.getData(&blImageData);
    
    // Check for null pixel data
    if (!blImageData.pixelData) {
        SDL_Log("PCBViewerWindow::UpdateTextureFromPcbRenderer: BLImage pixel data is null");
        return;
    }

    // Check if texture needs to be recreated (doesn't exist or size changed)
    bool createNewTexture = false;
    if (!m_renderTexture) {
        createNewTexture = true;
    } else {
        float currentWidth, currentHeight;
        if (SDL_GetTextureSize(m_renderTexture, &currentWidth, &currentHeight) == 0) {
            if (static_cast<int>(currentWidth) != blImageWidth || 
                static_cast<int>(currentHeight) != blImageHeight) {
                createNewTexture = true;
            }
        } else {
            // GetTextureSize failed, recreate to be safe
            createNewTexture = true;
        }
    }

    // Create new texture if needed
    if (createNewTexture) {
        // Release any old texture
        if (m_renderTexture) {
            SDL_DestroyTexture(m_renderTexture);
            m_renderTexture = nullptr;
        }

        // Create a new texture with the correct dimensions
        m_renderTexture = SDL_CreateTexture(
            sdlRenderer,
            SDL_PIXELFORMAT_ARGB8888,  // Matches Blend2D's BL_FORMAT_PRGB32
            SDL_TEXTUREACCESS_STREAMING, 
            blImageWidth,
            blImageHeight
        );

        if (!m_renderTexture) {
            SDL_Log("PCBViewerWindow: Failed to create texture: %s", SDL_GetError());
            return;
        }

        // Set blend mode for transparency - use premultiplied for Blend2D compatibility
        if (!SDL_SetTextureBlendMode(m_renderTexture, SDL_BLENDMODE_BLEND_PREMULTIPLIED)) {
            SDL_Log("PCBViewerWindow: Failed to set premultiplied blend mode: %s", SDL_GetError());
        }

        m_textureWidth = blImageWidth;
        m_textureHeight = blImageHeight;
    }

    // Update the texture data directly from Blend2D's buffer
    if (!SDL_UpdateTexture(m_renderTexture, NULL, blImageData.pixelData, blImageData.stride)) {
        SDL_Log("PCBViewerWindow: Failed to update texture: %s", SDL_GetError());
    }
}

// Main ImGui rendering function for this window, integrating PcbRenderer callback
void PCBViewerWindow::RenderIntegrated(SDL_Renderer* sdlRenderer, PcbRenderer* pcbRenderer, const std::function<void()>& pcbRenderCallback) {
    if (!m_isOpen && m_renderTexture) {
        ReleaseTexture(); 
        m_resizeCooldownFrames = -1; 
        m_desiredTextureSize = ImVec2(0,0); 
    }

    if (!m_isOpen) {
        // Reset focus/hover states if the window is not even supposed to be open
        m_isFocused = false;
        m_isHovered = false;
        m_isContentRegionHovered = false;
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    bool window_open_this_frame = ImGui::Begin(m_windowName.c_str(), &m_isOpen, window_flags);

    if (window_open_this_frame) {
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f)); // Ensure cursor is at the top-left of the content area

        m_isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        m_contentRegionSize = ImGui::GetContentRegionAvail();
        m_contentRegionTopLeftScreen = ImGui::GetCursorScreenPos(); 
        bool newSizeIsValid = (m_contentRegionSize.x > 0 && m_contentRegionSize.y > 0);

        // Update the shared viewport object 
        if (m_viewport) {
             m_viewport->SetDimensions(
                0, 0, 
                static_cast<int>(std::round(m_contentRegionSize.x)), 
                static_cast<int>(std::round(m_contentRegionSize.y))
            );
            // Inform PcbRenderer if its target image size needs to change.
            if (pcbRenderer && newSizeIsValid) {
                const BLImage& currentPcbImage = pcbRenderer->GetRenderedImage();
                int roundedWidth = static_cast<int>(std::round(m_contentRegionSize.x));
                int roundedHeight = static_cast<int>(std::round(m_contentRegionSize.y));

                if (currentPcbImage.width() != roundedWidth || 
                    currentPcbImage.height() != roundedHeight) {
                    pcbRenderer->OnViewportResized(roundedWidth, roundedHeight);
                }
            }
        }

        // Execute the PcbRenderer::Render() call via the callback
        if (pcbRenderCallback) {
            pcbRenderCallback();
        }

        // Update our SDL_Texture from PcbRenderer's BLImage (which should now be current)
        UpdateTextureFromPcbRenderer(sdlRenderer, pcbRenderer);

        if (m_renderTexture) {
            ImGui::Image(reinterpret_cast<ImTextureID>(m_renderTexture), ImVec2(m_textureWidth, m_textureHeight));
            
            // Handle interaction after the image is drawn, so ImGui::IsItemHovered() refers to the image
            if (m_interactionManager && (m_isFocused || m_isHovered)) {
                m_isContentRegionHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly); 
                m_interactionManager->ProcessInput(ImGui::GetIO(), m_isFocused, m_isContentRegionHovered, m_contentRegionTopLeftScreen, m_contentRegionSize);
            } else {
                m_isContentRegionHovered = false;
            }

        } else {
            ImGui::Text("PcbRenderer output not available or texture creation failed. Desired: (%.0f, %.0f)", m_contentRegionSize.x, m_contentRegionSize.y);
        }

    } else {
        // ImGui::Begin returned false (e.g. window collapsed, or m_isOpen was set to false by ImGui::Begin itself)
        m_isFocused = false;
        m_isHovered = false;
        m_isContentRegionHovered = false;
        if (m_renderTexture) { // If window is effectively closed by ImGui, release texture
            ReleaseTexture();
        }
    }
    
    // This PopStyleVar and End must be called if ImGui::Begin was called, 
    // regardless of whether it returned true or false, to keep ImGui state balanced.
    ImGui::End();
    ImGui::PopStyleVar();
}

void PCBViewerWindow::OnBoardLoaded(const std::shared_ptr<Board>& board) {
    if (board) {
        std::cout << "PCBViewerWindow: New board loaded: " << board->GetFilePath() << std::endl;
    }
    else {
        std::cout << "PCBViewerWindow: Board unloaded (or load failed)." << std::endl;
    }
} 
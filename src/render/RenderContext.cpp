#include "render/RenderContext.hpp"
#include <SDL3/SDL.h>
#include <stdexcept> // For std::runtime_error

// For Blend2D, if we integrate it here:
// #include <blend2d.h>

// For logging:
// #include <iostream>

RenderContext::RenderContext() : m_window(nullptr), m_renderer(nullptr) {
    // std::cout << "RenderContext created." << std::endl;
}

RenderContext::~RenderContext() {
    // Ensure Shutdown is called, though resources are ideally explicitly managed.
    if (m_renderer || m_window) { // Or just m_renderer if m_window is not owned
        // std::cerr << "RenderContext destroyed without calling Shutdown() first!" << std::endl;
        // Potentially call Shutdown() here, but it's better to ensure explicit cleanup.
        Shutdown(); 
    }
    // std::cout << "RenderContext destroyed." << std::endl;
}

bool RenderContext::Initialize(SDL_Window* window) {
    if (!window) {
        SDL_Log("RenderContext::Initialize Error: Provided window is null.\n");
        return false;
    }
    m_window = window;

    // Create SDL Renderer
    // You might want to specify flags, e.g., SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        SDL_Log("RenderContext::Initialize Error: Failed to create SDL_Renderer: %s\n", SDL_GetError());
        m_window = nullptr; // Don't keep a pointer to the window if renderer creation failed in a way that makes it unusable.
        return false;
    }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    // Initialize Blend2D context if used here
    // BLResult err = m_blContext.create(m_renderer, SDL_GetWindowSurface(m_window)->format);
    // if (err != BL_SUCCESS) {
    //     SDL_Log("RenderContext::Initialize Error: Failed to create Blend2D context: %u\n", err);
    //     SDL_DestroyRenderer(m_renderer);
    //     m_renderer = nullptr;
    //     m_window = nullptr;
    //     return false;
    // }

    // std::cout << "RenderContext initialized." << std::endl;
    return true;
}

void RenderContext::Shutdown() {
    // if (m_blContext) {
    //    m_blContext.destroy();
    //    // m_blContext = nullptr; // If it's a raw pointer or needs explicit nulling
    // }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    // m_window is not owned by RenderContext, so it's not destroyed here.
    m_window = nullptr; 
    // std::cout << "RenderContext shutdown." << std::endl;
}

void RenderContext::BeginFrame() {
    // Example: Clear the screen to a default color (e.g., black or a specific clear color)
    // SDL_SetRenderDrawColor(m_renderer, (Uint8)(m_clearColor[0]*255), (Uint8)(m_clearColor[1]*255), (Uint8)(m_clearColor[2]*255), (Uint8)(m_clearColor[3]*255));
    // SDL_RenderClear(m_renderer);
    // Or, if Blend2D is handling the surface, its own clear might be used.
    // if (m_blContext) { /* m_blContext.clearAll(); or similar */ }
}

void RenderContext::EndFrame() {
    // SDL_RenderPresent(m_renderer); // This is typically done by the main loop after ImGui rendering
}

SDL_Renderer* RenderContext::GetRenderer() const {
    return m_renderer;
}

// BLContextCore* RenderContext::GetBlend2DContext() const {
//     return m_blContext; // or &m_blContext if it's not a pointer
// }

// void RenderContext::SetClearColor(float r, float g, float b, float a) {
//     m_clearColor[0] = r;
//     m_clearColor[1] = g;
//     m_clearColor[2] = b;
//     m_clearColor[3] = a;
// }

// void RenderContext::Clear() {
//     if (m_renderer) {
//          SDL_SetRenderDrawColor(m_renderer, (Uint8)(m_clearColor[0]*255), (Uint8)(m_clearColor[1]*255), (Uint8)(m_clearColor[2]*255), (Uint8)(m_clearColor[3]*255));
//          SDL_RenderClear(m_renderer);
//     }
// } 
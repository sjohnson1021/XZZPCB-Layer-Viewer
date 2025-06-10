#include "SDLRenderer.hpp"

#include <iostream>

SDLRenderer::SDLRenderer() : m_window_(nullptr), m_renderer_(nullptr) {}

SDLRenderer::~SDLRenderer()
{
    Shutdown();
}

bool SDLRenderer::Initialize(const std::string& title, int width, int height)
{
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "Error: SDL_Init(): " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "SDL initialized successfully" << std::endl;

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    m_window_ = SDL_CreateWindow(title.c_str(), width, height, window_flags);
    if (m_window_ == nullptr) {
        std::cerr << "Error: SDL_CreateWindow(): " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    std::cout << "Window created successfully" << std::endl;

    // Create renderer with hardware acceleration
    // SDL3 doesn't use explicit renderer flags like SDL2 did
    m_renderer_ = SDL_CreateRenderer(m_window_, nullptr);
    if (m_renderer_ == nullptr) {
        std::cerr << "Error: SDL_CreateRenderer(): " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(m_window_);
        SDL_Quit();
        return false;
    }

    std::cout << "Renderer created successfully" << std::endl;

    // Enable VSync for optimal performance on lower-end hardware
    SDL_SetRenderVSync(m_renderer_, 1);
    std::cout << "VSync enabled for optimal performance" << std::endl;

    // Set window position and show it
    SDL_SetWindowPosition(m_window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(m_window_);

    std::cout << "Window shown successfully" << std::endl;
    return true;
}

void SDLRenderer::Shutdown()
{
    if (m_renderer_) {
        SDL_DestroyRenderer(m_renderer_);
        m_renderer_ = nullptr;
    }

    if (m_window_) {
        SDL_DestroyWindow(m_window_);
        m_window_ = nullptr;
    }

    SDL_Quit();
}

void SDLRenderer::Clear()
{
    SDL_SetRenderDrawColorFloat(m_renderer_, 0.2f, 0.2f, 0.2f, 1.0f);
    SDL_RenderClear(m_renderer_);
}

void SDLRenderer::Present()
{
    SDL_RenderPresent(m_renderer_);
}

void* SDLRenderer::GetWindowHandle() const
{
    return m_window_;
}

void* SDLRenderer::GetRendererHandle() const
{
    return m_renderer_;
}

int SDLRenderer::GetWindowWidth() const
{
    int width;
    SDL_GetWindowSize(m_window_, &width, nullptr);
    return width;
}

int SDLRenderer::GetWindowHeight() const
{
    int height;
    SDL_GetWindowSize(m_window_, nullptr, &height);
    return height;
}

bool SDLRenderer::IsValid() const
{
    // Check if both window and renderer are still valid
    if (!m_window_ || !m_renderer_) {
        return false;
    }

    // Check if the window is still valid by querying its properties
    SDL_WindowFlags flags = SDL_GetWindowFlags(m_window_);
    if (flags == 0) {
        return false;
    }

    // Check if window is minimized or hidden, which might affect renderer validity
    if (flags & SDL_WINDOW_MINIMIZED) {
        // Window is minimized, renderer might be invalid
        return false;
    }

    // Try a simple operation to verify renderer is still functional
    // This is a lightweight check that should work in SDL3
    int width, height;
    if (SDL_GetRenderOutputSize(m_renderer_, &width, &height) != 0) {
        return false;
    }

    return true;
}

bool SDLRenderer::Recreate()
{
    std::cout << "SDLRenderer: Attempting to recreate renderer..." << std::endl;

    if (!m_window_) {
        std::cerr << "SDLRenderer::Recreate Error: Window is null, cannot recreate renderer" << std::endl;
        return false;
    }

    // Destroy the old renderer if it exists
    if (m_renderer_) {
        SDL_DestroyRenderer(m_renderer_);
        m_renderer_ = nullptr;
    }

    // Create a new renderer
    m_renderer_ = SDL_CreateRenderer(m_window_, nullptr);
    if (m_renderer_ == nullptr) {
        std::cerr << "SDLRenderer::Recreate Error: SDL_CreateRenderer(): " << SDL_GetError() << std::endl;
        return false;
    }

    // Re-enable VSync
    SDL_SetRenderVSync(m_renderer_, 1);

    std::cout << "SDLRenderer: Successfully recreated renderer" << std::endl;
    return true;
}

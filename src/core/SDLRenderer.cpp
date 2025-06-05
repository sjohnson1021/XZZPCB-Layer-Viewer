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

    // Enable vsync for smoother rendering
    SDL_SetRenderVSync(m_renderer_, 1);

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

#include "SDLRenderer.hpp"
#include <iostream>

SDLRenderer::SDLRenderer()
    : m_window(nullptr)
    , m_renderer(nullptr)
{
}

SDLRenderer::~SDLRenderer()
{
    Shutdown();
}

bool SDLRenderer::Initialize(const std::string& title, int width, int height)
{
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        std::cerr << "Error: SDL_Init(): " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "SDL initialized successfully" << std::endl;

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    m_window = SDL_CreateWindow(title.c_str(), width, height, window_flags);
    if (m_window == nullptr)
    {
        std::cerr << "Error: SDL_CreateWindow(): " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    std::cout << "Window created successfully" << std::endl;

    // Create renderer with hardware acceleration
    // SDL3 doesn't use explicit renderer flags like SDL2 did
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (m_renderer == nullptr)
    {
        std::cerr << "Error: SDL_CreateRenderer(): " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        return false;
    }

    std::cout << "Renderer created successfully" << std::endl;
    
    // Enable vsync for smoother rendering
    SDL_SetRenderVSync(m_renderer, 1);

    // Set window position and show it
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(m_window);

    std::cout << "Window shown successfully" << std::endl;
    return true;
}

void SDLRenderer::Shutdown()
{
    if (m_renderer)
    {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }

    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();
}

void SDLRenderer::Clear()
{
    SDL_SetRenderDrawColorFloat(m_renderer, 0.2f, 0.2f, 0.2f, 1.0f);
    SDL_RenderClear(m_renderer);
}

void SDLRenderer::Present()
{
    SDL_RenderPresent(m_renderer);
}

void* SDLRenderer::GetWindowHandle() const {
    return m_window;
}

void* SDLRenderer::GetRendererHandle() const {
    return m_renderer;
}

int SDLRenderer::GetWindowWidth() const {
    int width;
    SDL_GetWindowSize(m_window, &width, nullptr);
    return width;
}

int SDLRenderer::GetWindowHeight() const {
    int height;
    SDL_GetWindowSize(m_window, nullptr, &height);
    return height;
} 
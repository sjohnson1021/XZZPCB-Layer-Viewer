#pragma once

#include "Renderer.hpp"
#include <SDL3/SDL.h>
#include <string>

class SDLRenderer : public Renderer {
public:
    SDLRenderer();
    ~SDLRenderer() override;

    bool Initialize(const std::string& title, int width, int height) override;
    void Shutdown() override;
    void Clear() override;
    void Present() override;

    // SDL-specific accessors
    SDL_Window* GetWindow() const { return m_window; }
    SDL_Renderer* GetRenderer() const { return m_renderer; }
    void* GetWindowHandle() const;
    void* GetRendererHandle() const;
    int GetWindowWidth() const;
    int GetWindowHeight() const;

private:
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
}; 
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
    [[nodiscard]] SDL_Window* GetWindow() const { return m_window_; }
    [[nodiscard]] SDL_Renderer* GetRenderer() const { return m_renderer_; }
    [[nodiscard]] void* GetWindowHandle() const override;
    [[nodiscard]] void* GetRendererHandle() const override;
    [[nodiscard]] int GetWindowWidth() const override;
    [[nodiscard]] int GetWindowHeight() const override;

    // Window event handling methods
    [[nodiscard]] bool IsValid() const;
    bool Recreate();

private:
    SDL_Window* m_window_;
    SDL_Renderer* m_renderer_;
};
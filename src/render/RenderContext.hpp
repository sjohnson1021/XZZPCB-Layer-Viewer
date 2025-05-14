#pragma once

// Forward declarations
struct SDL_Window;
struct SDL_Renderer;
// Potentially Blend2D context if we manage it here
// struct BLContextCore;

class RenderContext {
public:
    RenderContext();
    ~RenderContext();

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;
    RenderContext(RenderContext&&) = delete;
    RenderContext& operator=(RenderContext&&) = delete;

    // Initialization might take SDL_Window* if it's not created/managed internally
    bool Initialize(SDL_Window* window);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    // Accessors for rendering resources
    SDL_Renderer* GetRenderer() const;
    // BLContextCore* GetBlend2DContext() const; // If using Blend2D directly

    // Other state management methods
    // void SetClearColor(float r, float g, float b, float a);
    // void Clear();

private:
    SDL_Window* m_window = nullptr;         // Raw pointer, ownership likely managed elsewhere (e.g., Application class)
    SDL_Renderer* m_renderer = nullptr;   // Managed by this class if created here, or raw if passed in
    // BLContextCore* m_blContext = nullptr; // Blend2D context

    // Potentially other state variables:
    // float m_clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
}; 
#pragma once

#include <blend2d.h> // For BLImage and BLContext
#include <memory>    // For std::unique_ptr

// Forward declarations (if any become necessary)
// struct SDL_Window; // No longer needed directly by RenderContext

class RenderContext {
public:
    RenderContext();
    ~RenderContext();

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;
    RenderContext(RenderContext&&) = delete;
    RenderContext& operator=(RenderContext&&) = delete;

    // Initialize with a default size, possibly configurable later
    bool Initialize(int width, int height);
    void Shutdown();

    // Frame operations for the Blend2D context
    void BeginFrame(); // Clears the BLImage
    void EndFrame();   // Finalizes BLContext operations if any (might be a no-op)

    // Accessors for Blend2D resources
    BLContext& GetBlend2DContext();
    const BLImage& GetTargetImage() const; // Read-only access to the image
    BLImage& GetTargetImage();             // Writable access if needed, e.g., for direct manipulation or resizing

    // Potentially a method to resize the off-screen image
    bool ResizeImage(int newWidth, int newHeight);

private:
    // Blend2D resources
    BLImage m_targetImage;       // The off-screen image for PCB rendering
    BLContext m_blContext;       // Blend2D rendering context targeting m_targetImage
    // No longer managing SDL_Window* or SDL_Renderer*
    // SDL_Window* m_window = nullptr;
    // SDL_Renderer* m_renderer = nullptr;

    int m_imageWidth = 0;
    int m_imageHeight = 0;
}; 
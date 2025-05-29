#pragma once

#include <blend2d.h> // For BLImage and BLContext
#include <memory>    // For std::unique_ptr
#include "core/BoardDataManager.hpp"

// Forward declarations (if any become necessary)
// struct SDL_Window; // No longer needed directly by RenderContext

class RenderContext
{
public:
    RenderContext();
    ~RenderContext();

    RenderContext(const RenderContext &) = delete;
    RenderContext &operator=(const RenderContext &) = delete;
    RenderContext(RenderContext &&) = delete;
    RenderContext &operator=(RenderContext &&) = delete;

    // Initialize with a default size, possibly configurable later
    bool Initialize(int width, int height);
    void Shutdown();

    // Frame operations for the Blend2D context
    void BeginFrame(); // Clears the BLImage
    void EndFrame();   // Finalizes BLContext operations if any (might be a no-op)

    // Accessors for Blend2D resources
    BLContext &GetBlend2DContext();
    const BLImage &GetTargetImage() const; // Read-only access to the image
    BLImage &GetTargetImage();             // Writable access if needed, e.g., for direct manipulation or resizing

    // Add public getters for image dimensions
    int GetImageWidth() const { return m_imageWidth; }
    int GetImageHeight() const { return m_imageHeight; }

    // Potentially a method to resize the off-screen image
    bool ResizeImage(int newWidth, int newHeight);

    // Set the clear color for BeginFrame
    void SetClearColor(float r, float g, float b, float a = 1.0f)
    {
        m_clearColor[0] = r;
        m_clearColor[1] = g;
        m_clearColor[2] = b;
        m_clearColor[3] = a;
    }

    // Control whether BeginFrame does a full clear
    void SetClearOnBeginFrame(bool shouldClear)
    {
        m_clearOnBeginFrame = shouldClear;
    }

    // Performance optimization methods
    void OptimizeForStatic();
    void OptimizeForInteractive();

    // BoardDataManager access
    void SetBoardDataManager(std::shared_ptr<BoardDataManager> boardDataManager);
    std::shared_ptr<BoardDataManager> GetBoardDataManager() const;

private:
    // Blend2D resources
    BLImage m_targetImage; // The off-screen image for PCB rendering
    BLContext m_blContext; // Blend2D rendering context targeting m_targetImage
    // No longer managing SDL_Window* or SDL_Renderer*
    // SDL_Window* m_window = nullptr;
    // SDL_Renderer* m_renderer = nullptr;

    int m_imageWidth = 0;
    int m_imageHeight = 0;
    float m_clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // Default clear color (transparent black)
    bool m_clearOnBeginFrame = true;                  // Whether to clear on BeginFrame
    std::shared_ptr<BoardDataManager> m_boardDataManager;
};
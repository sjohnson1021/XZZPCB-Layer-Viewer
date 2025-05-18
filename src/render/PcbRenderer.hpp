#pragma once

#include <memory>
#include <blend2d.h> // For BLImage

// Forward declarations
class RenderContext; // The Blend2D-focused RenderContext
class RenderPipeline;
class Board;
class Camera;
class Viewport;
class Grid; // Added forward declaration for Grid

class PcbRenderer {
public:
    PcbRenderer();
    ~PcbRenderer();

    PcbRenderer(const PcbRenderer&) = delete;
    PcbRenderer& operator=(const PcbRenderer&) = delete;
    PcbRenderer(PcbRenderer&&) = delete;
    PcbRenderer& operator=(PcbRenderer&&) = delete;

    // Initialize with default dimensions for the internal RenderContext's BLImage
    bool Initialize(int initialWidth, int initialHeight);
    void Shutdown();

    // Main rendering method
    // Renders the PCB and grid to an internal BLImage managed by RenderContext.
    void Render(const Board* board, const Camera* camera, const Viewport* viewport, const Grid* grid); // Added Grid

    // Access the rendered image
    const BLImage& GetRenderedImage() const;
    // Potentially a method to notify of viewport size changes to resize the BLImage
    void OnViewportResized(int newWidth, int newHeight);


private:
    std::unique_ptr<RenderContext> m_renderContext;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    // std::vector<IRenderable*> m_renderables; // If using a list of renderables
}; 
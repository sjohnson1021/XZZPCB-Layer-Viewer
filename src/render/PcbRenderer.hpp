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

class PcbRenderer
{
public:
    PcbRenderer();
    ~PcbRenderer();

    PcbRenderer(const PcbRenderer &) = delete;
    PcbRenderer &operator=(const PcbRenderer &) = delete;
    PcbRenderer(PcbRenderer &&) = delete;
    PcbRenderer &operator=(PcbRenderer &&) = delete;

    // Initialize with default dimensions for the internal RenderContext's BLImage
    bool Initialize(int initialWidth, int initialHeight);
    void Shutdown();

    // Main rendering method
    // Renders the PCB and grid to an internal BLImage managed by RenderContext.
    void Render(const Board *board, const Camera *camera, const Viewport *viewport, const Grid *grid); // Added Grid

    // Access the rendered image
    const BLImage &GetRenderedImage() const;
    // Potentially a method to notify of viewport size changes to resize the BLImage
    void OnViewportResized(int newWidth, int newHeight);

    // Methods for conditional rendering
    void MarkGridDirty()
    {
        m_gridDirty = true;
        m_needsRedrawSignal = true;
    }
    void MarkBoardDirty()
    {
        m_boardDirty = true;
        m_needsRedrawSignal = true;
    }
    void NotifyViewportResizedEvent()
    {
        m_viewportResizedSignal = true;
        m_needsRedrawSignal = true;
    } // Renamed to avoid conflict
    bool WasFrameJustRendered() const { return m_frameRenderedThisCycle; }
    bool NeedsRedraw() const { return m_needsRedrawSignal; }

private:
    std::unique_ptr<RenderContext> m_renderContext;
    std::unique_ptr<RenderPipeline> m_renderPipeline;
    // std::vector<IRenderable*> m_renderables; // If using a list of renderables

    // Flags for conditional rendering
    bool m_gridDirty = true;
    bool m_boardDirty = true;
    bool m_fullRedrawNeeded = true;        // True if viewport resize or initial render
    bool m_frameRenderedThisCycle = false; // True if Render() actually drew something
    bool m_viewportResizedSignal = false;  // Signal that viewport has been resized
    bool m_needsRedrawSignal = true;       // Master signal if any part needs redraw

    // For OptimizeForStatic/Interactive switching
    int m_interactiveFramesCounter = 0;
    static const int INTERACTIVE_THRESHOLD = 2; // Frames of continuous camera change
    bool m_isInteractiveOptimized = false;
};
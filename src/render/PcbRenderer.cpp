#include "render/PcbRenderer.hpp"
#include "render/RenderContext.hpp"
#include "render/RenderPipeline.hpp"
#include "pcb/Board.hpp"     // For board data
#include "view/Camera.hpp"   // For camera parameters
#include "view/Viewport.hpp" // For viewport parameters
#include "view/Grid.hpp"     // For Grid rendering, if PcbRenderer calls it directly
                             // Otherwise, RenderPipeline might handle Grid.
#include <iostream>          // For logging

PcbRenderer::PcbRenderer()
    : m_gridDirty(true), m_boardDirty(true), m_fullRedrawNeeded(true), m_frameRenderedThisCycle(false), m_viewportResizedSignal(false), m_needsRedrawSignal(true), m_interactiveFramesCounter(0), m_isInteractiveOptimized(false)
{
    // Constructor implementation
    // std::cout << "PcbRenderer created." << std::endl;
}

PcbRenderer::~PcbRenderer()
{
    Shutdown();
    // std::cout << "PcbRenderer destroyed." << std::endl;
}

bool PcbRenderer::Initialize(int initialWidth, int initialHeight, std::shared_ptr<BoardDataManager> boardDataManager)
{
    m_boardDataManager = boardDataManager;
    m_renderContext = std::make_unique<RenderContext>();
    if (!m_renderContext->Initialize(initialWidth, initialHeight))
    {
        std::cerr << "PcbRenderer Error: Failed to initialize RenderContext." << std::endl;
        m_renderContext.reset();
        return false;
    }
    m_renderContext->OptimizeForStatic(); // Default to static
    m_isInteractiveOptimized = false;
    m_renderPipeline = std::make_unique<RenderPipeline>();
    if (!m_renderPipeline->Initialize(*m_renderContext))
    { // Pass the RenderContext by reference
        std::cerr << "PcbRenderer Error: Failed to initialize RenderPipeline." << std::endl;
        m_renderContext->Shutdown();
        m_renderContext.reset();
        m_renderPipeline.reset();
        return false;
    }

    // Set the BoardDataManager in the RenderContext
    m_renderContext->SetBoardDataManager(m_boardDataManager);

    // Register for NetID change callbacks
    if (m_boardDataManager)
    {
        m_boardDataManager->RegisterNetIdChangeCallback([this](int netId)
                                                        { this->MarkBoardDirty(); this->MarkGridDirty(); });
        m_boardDataManager->RegisterSettingsChangeCallback([this]()
                                                           { this->MarkBoardDirty(); this->MarkGridDirty(); });
    }

    std::cout << "PcbRenderer initialized." << std::endl;
    return true;
}

void PcbRenderer::Shutdown()
{
    // Unregister the callbacks before shutting down
    if (m_boardDataManager)
    {
        m_boardDataManager->UnregisterNetIdChangeCallback();
        m_boardDataManager->UnregisterSettingsChangeCallback();
    }

    if (m_renderPipeline)
    {
        m_renderPipeline->Shutdown();
        m_renderPipeline.reset();
    }
    if (m_renderContext)
    {
        m_renderContext->Shutdown();
        m_renderContext.reset();
    }
    // std::cout << "PcbRenderer shutdown." << std::endl;
}

void PcbRenderer::Render(const Board *board, const Camera *camera, const Viewport *viewport, const Grid *grid)
{
    m_frameRenderedThisCycle = false; // Reset at the start of each Render call

    if (camera)
    { // Check if camera is valid before using it
        if (camera->WasViewChangedThisFrame())
        { // This flag is true if camera moved this frame (before InteractionManager cleared it)
            if (m_interactiveFramesCounter < INTERACTIVE_THRESHOLD)
            {
                m_interactiveFramesCounter++;
            }
            if (m_interactiveFramesCounter >= INTERACTIVE_THRESHOLD && !m_isInteractiveOptimized)
            {
                if (m_renderContext)
                    m_renderContext->OptimizeForInteractive();
                m_isInteractiveOptimized = true;
                // std::cout << "Switched to OptimizeForInteractive" << std::endl;
            }
        }
        else
        {
            if (m_interactiveFramesCounter > 0)
            {
                m_interactiveFramesCounter = 0;
            }
            if (m_isInteractiveOptimized)
            {
                if (m_renderContext)
                    m_renderContext->OptimizeForStatic();
                m_isInteractiveOptimized = false;
                // std::cout << "Switched to OptimizeForStatic" << std::endl;
            }
        }
    }
    else
    { // No camera, ensure static optimization and reset counters
        if (m_isInteractiveOptimized)
        {
            if (m_renderContext)
                m_renderContext->OptimizeForStatic();
            m_isInteractiveOptimized = false;
        }
        m_interactiveFramesCounter = 0;
    }

    if (!m_renderContext || !m_renderPipeline)
    {
        std::cerr << "PcbRenderer Error: Not initialized (context or pipeline missing)." << std::endl;
        return;
    }

    if (!camera || !viewport || !grid)
    {
        // Only redraw placeholder if needed or if it's the first time
        if (m_needsRedrawSignal)
        {
            m_renderContext->BeginFrame();
            BLContext &ctx = m_renderContext->GetBlend2DContext();
            ctx.fillAll(BLRgba32(0xFF111111));
            std::cerr << "PcbRenderer::Render Warning: Missing critical components (camera, viewport, or grid). Rendering placeholder." << std::endl;
            if (!camera)
                std::cerr << "  - Camera is null" << std::endl;
            if (!viewport)
                std::cerr << "  - Viewport is null" << std::endl;
            if (!grid)
                std::cerr << "  - Grid is null" << std::endl;
            m_renderContext->EndFrame();
            m_frameRenderedThisCycle = true;
            m_needsRedrawSignal = false; // Consumed the redraw signal
        }
        return;
    }

    int viewportWidth = viewport->GetWidth();
    int viewportHeight = viewport->GetHeight();

    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        std::cerr << "PcbRenderer::Render Error: Invalid viewport dimensions ("
                  << viewportWidth << "x" << viewportHeight << "). Skipping render." << std::endl;
        // Optionally, render a small placeholder or clear the existing image if it's differently sized
        if (m_needsRedrawSignal && m_renderContext->GetImageWidth() > 0 && m_renderContext->GetImageHeight() > 0)
        { // Check if context has an image
            m_renderContext->BeginFrame();
            m_renderContext->GetBlend2DContext().fillAll(BLRgba32(0xFF000000)); // Fill black
            m_renderContext->EndFrame();
            m_frameRenderedThisCycle = true;
            m_needsRedrawSignal = false; // Consumed the redraw signal
        }
        return;
    }

    if (m_viewportResizedSignal)
    {
        std::cout << "PcbRenderer::Render: Viewport size (" << viewportWidth << "x" << viewportHeight
                  << ") differs from RenderContext image size (" << m_renderContext->GetImageWidth()
                  << "x" << m_renderContext->GetImageHeight() << "). Resizing context." << std::endl;
        if (!m_renderContext->ResizeImage(viewportWidth, viewportHeight))
        {
            std::cerr << "PcbRenderer::Render Error: Failed to resize RenderContext image to match viewport. Skipping render." << std::endl;
            m_viewportResizedSignal = false; // Acknowledge signal even on failure
            return;
        }
        m_fullRedrawNeeded = true;       // Force full redraw after resize
        m_viewportResizedSignal = false; // Acknowledge signal
    }

    // If nothing needs to be redrawn, just return.
    if (!m_gridDirty && !m_boardDirty && !m_fullRedrawNeeded && !m_needsRedrawSignal)
    {
        return;
    }

    m_renderContext->BeginFrame();
    BLContext &bl_ctx = m_renderContext->GetBlend2DContext();

    try
    {
        // Pass the dirty flags to the pipeline execution
        m_renderPipeline->BeginScene(bl_ctx);
        m_renderPipeline->Execute(bl_ctx, board, *camera, *viewport, *grid, m_gridDirty || m_fullRedrawNeeded, m_boardDirty || m_fullRedrawNeeded);
        m_renderPipeline->EndScene();

        // Reset flags after successful rendering
        m_gridDirty = false;
        m_boardDirty = false;
        m_fullRedrawNeeded = false;
        m_frameRenderedThisCycle = true;
        m_needsRedrawSignal = false; // Consumed the redraw signal
    }
    catch (const std::exception &e)
    {
        std::cerr << "PcbRenderer::Render Exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "PcbRenderer::Render: Unknown exception during rendering" << std::endl;
    }

    m_renderContext->EndFrame();
}

const BLImage &PcbRenderer::GetRenderedImage() const
{
    if (!m_renderContext)
    {
        // This case should ideally not happen if used correctly (after init)
        // Return a static empty BLImage or throw an exception
        static BLImage emptyImage;
        std::cerr << "PcbRenderer Error: GetRenderedImage called when RenderContext is null." << std::endl;
        return emptyImage;
    }

    // Return the rendered image - no need for verbose logging every frame
    return m_renderContext->GetTargetImage();
}

void PcbRenderer::OnViewportResized(int newWidth, int newHeight)
{
    // This function is called by PCBViewerWindow when ImGui reports a content region size change.
    // We signal the Render() method to handle the actual resize and full redraw.
    if (m_renderContext)
    {
        if (m_renderContext->GetImageWidth() != newWidth || m_renderContext->GetImageHeight() != newHeight)
        {
            NotifyViewportResizedEvent(); // Signal that viewport resize is needed
                                          // The actual m_renderContext->ResizeImage will happen in the Render() method
                                          // based on m_viewportResizedSignal
        }
    }
    else
    {
        std::cerr << "PcbRenderer Warning: OnViewportResized called but RenderContext is not initialized." << std::endl;
    }
}
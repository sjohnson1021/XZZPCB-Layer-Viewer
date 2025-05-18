#include "render/PcbRenderer.hpp"
#include "render/RenderContext.hpp"
#include "render/RenderPipeline.hpp"
#include "pcb/Board.hpp"          // For board data
#include "view/Camera.hpp"        // For camera parameters
#include "view/Viewport.hpp"      // For viewport parameters
#include "view/Grid.hpp"          // For Grid rendering, if PcbRenderer calls it directly
                                  // Otherwise, RenderPipeline might handle Grid.
#include <iostream> // For logging

PcbRenderer::PcbRenderer() {
    // Constructor implementation
    // std::cout << "PcbRenderer created." << std::endl;
}

PcbRenderer::~PcbRenderer() {
    Shutdown();
    // std::cout << "PcbRenderer destroyed." << std::endl;
}

bool PcbRenderer::Initialize(int initialWidth, int initialHeight) {
    m_renderContext = std::make_unique<RenderContext>();
    if (!m_renderContext->Initialize(initialWidth, initialHeight)) {
        std::cerr << "PcbRenderer Error: Failed to initialize RenderContext." << std::endl;
        m_renderContext.reset();
        return false;
    }

    m_renderPipeline = std::make_unique<RenderPipeline>();
    if (!m_renderPipeline->Initialize(*m_renderContext)) { // Pass the RenderContext by reference
        std::cerr << "PcbRenderer Error: Failed to initialize RenderPipeline." << std::endl;
        m_renderContext->Shutdown();
        m_renderContext.reset();
        m_renderPipeline.reset();
        return false;
    }
    std::cout << "PcbRenderer initialized." << std::endl;
    return true;
}

void PcbRenderer::Shutdown() {
    if (m_renderPipeline) {
        m_renderPipeline->Shutdown();
        m_renderPipeline.reset();
    }
    if (m_renderContext) {
        m_renderContext->Shutdown();
        m_renderContext.reset();
    }
    // std::cout << "PcbRenderer shutdown." << std::endl;
}

void PcbRenderer::Render(const Board* board, const Camera* camera, const Viewport* viewport, const Grid* grid) {
    if (!m_renderContext || !m_renderPipeline) {
        std::cerr << "PcbRenderer Error: Not initialized (context or pipeline missing)." << std::endl;
        return;
    }

    if (!camera || !viewport || !grid) {
        m_renderContext->BeginFrame(); 
        BLContext& ctx = m_renderContext->GetBlend2DContext();
        ctx.fillAll(BLRgba32(0xFF111111)); 
        std::cerr << "PcbRenderer::Render Warning: Missing critical components (camera, viewport, or grid). Rendering placeholder." << std::endl;
        if (!camera) std::cerr << "  - Camera is null" << std::endl;
        if (!viewport) std::cerr << "  - Viewport is null" << std::endl;
        if (!grid) std::cerr << "  - Grid is null" << std::endl;
        m_renderContext->EndFrame();
        return;
    }

    // Ensure RenderContext's image matches the current viewport dimensions
    int viewportWidth = viewport->GetWidth();
    int viewportHeight = viewport->GetHeight();

    if (viewportWidth <= 0 || viewportHeight <= 0) {
        std::cerr << "PcbRenderer::Render Error: Invalid viewport dimensions (" 
                  << viewportWidth << "x" << viewportHeight << "). Skipping render." << std::endl;
        // Optionally, render a small placeholder or clear the existing image if it's differently sized
        if (m_renderContext->GetImageWidth() > 0 && m_renderContext->GetImageHeight() > 0) { // Check if context has an image
             m_renderContext->BeginFrame();
             m_renderContext->GetBlend2DContext().fillAll(BLRgba32(0xFF000000)); // Fill black
             m_renderContext->EndFrame();
        }
        return;
    }

    if (m_renderContext->GetImageWidth() != viewportWidth ||
        m_renderContext->GetImageHeight() != viewportHeight) {
        std::cout << "PcbRenderer::Render: Viewport size (" << viewportWidth << "x" << viewportHeight 
                  << ") differs from RenderContext image size (" << m_renderContext->GetImageWidth() 
                  << "x" << m_renderContext->GetImageHeight() << "). Resizing context." << std::endl;
        if (!m_renderContext->ResizeImage(viewportWidth, viewportHeight)) {
            std::cerr << "PcbRenderer::Render Error: Failed to resize RenderContext image to match viewport. Skipping render." << std::endl;
            return; // Critical error, cannot proceed with rendering
        } // After a successful resize, the RenderContext's BLImage is ready and its context is already re-begun.
    }

    m_renderContext->BeginFrame(); 
    BLContext& bl_ctx = m_renderContext->GetBlend2DContext();

    try {
        m_renderPipeline->BeginScene(bl_ctx);
        m_renderPipeline->Execute(bl_ctx, board, *camera, *viewport, *grid);
        m_renderPipeline->EndScene();
    } catch (const std::exception& e) {
        std::cerr << "PcbRenderer::Render Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "PcbRenderer::Render: Unknown exception during rendering" << std::endl;
    }

    m_renderContext->EndFrame();
}

const BLImage& PcbRenderer::GetRenderedImage() const {
    if (!m_renderContext) {
        // This case should ideally not happen if used correctly (after init)
        // Return a static empty BLImage or throw an exception
        static BLImage emptyImage; 
        std::cerr << "PcbRenderer Error: GetRenderedImage called when RenderContext is null." << std::endl;
        return emptyImage; 
    }
    
    // Return the rendered image - no need for verbose logging every frame
    return m_renderContext->GetTargetImage();
}

void PcbRenderer::OnViewportResized(int newWidth, int newHeight) {
    if (m_renderContext) {
        if (!m_renderContext->ResizeImage(newWidth, newHeight)) {
            std::cerr << "PcbRenderer Error: Failed to resize RenderContext image." << std::endl;
            // Handle error, perhaps by trying to reinitialize or marking as invalid state
        }
    } else {
        std::cerr << "PcbRenderer Warning: OnViewportResized called but RenderContext is not initialized." << std::endl;
    }
} 
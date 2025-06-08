#include "render/PcbRenderer.hpp"

#include "pcb/Board.hpp"  // For board data
#include "render/RenderContext.hpp"
#include "render/RenderPipeline.hpp"
#include "view/Camera.hpp"    // For camera parameters
#include "view/Grid.hpp"      // For Grid rendering, if PcbRenderer calls it directly
#include "view/Viewport.hpp"  // For viewport parameters
                              // Otherwise, RenderPipeline might handle Grid.
#include <iostream>

PcbRenderer::PcbRenderer()
    : m_grid_dirty_(true),
      m_board_dirty_(true),
      m_full_redraw_needed_(true),
      m_frame_rendered_this_cycle_(false),
      m_viewport_resized_signal_(false),
      m_needs_redraw_signal_(true),
      m_interactive_frames_counter_(0),
      m_is_interactive_optimized_(false)
{
    // Constructor implementation
    // std::cout << "PcbRenderer created." << std::endl;
}

PcbRenderer::~PcbRenderer()
{
    Shutdown();
    // std::cout << "PcbRenderer destroyed." << std::endl;
}

bool PcbRenderer::Initialize(int initial_width, int initial_height, std::shared_ptr<BoardDataManager> board_data_manager)
{
    m_board_data_manager_ = board_data_manager;
    m_render_context_ = std::make_unique<RenderContext>();
    if (!m_render_context_->Initialize(initial_width, initial_height)) {
        std::cerr << "PcbRenderer Error: Failed to initialize RenderContext." << std::endl;
        m_render_context_.reset();
        return false;
    }
    m_render_context_->OptimizeForStatic();  // Default to static
    m_is_interactive_optimized_ = false;
    m_render_pipeline_ = std::make_unique<RenderPipeline>();
    if (!m_render_pipeline_->Initialize(*m_render_context_)) {  // Pass the RenderContext by reference
        std::cerr << "PcbRenderer Error: Failed to initialize RenderPipeline." << std::endl;
        m_render_context_->Shutdown();
        m_render_context_.reset();
        m_render_pipeline_.reset();
        return false;
    }

    // Set the BoardDataManager in the RenderContext
    m_render_context_->SetBoardDataManager(m_board_data_manager_);

    // Register for NetID change callbacks
    if (m_board_data_manager_) {
        m_board_data_manager_->RegisterNetIdChangeCallback([this](int net_id) {
            this->MarkBoardDirty();
            this->MarkGridDirty();
        });
        m_board_data_manager_->RegisterSettingsChangeCallback([this]() {
            this->MarkBoardDirty();
            this->MarkGridDirty();
        });
        m_board_data_manager_->RegisterLayerVisibilityChangeCallback([this](int layer_id, bool visible) {
            this->MarkBoardDirty();  // Layer visibility changes require board redraw
        });
    }

    std::cout << "PcbRenderer initialized." << std::endl;
    return true;
}

void PcbRenderer::Shutdown()
{
    // Unregister the callbacks before shutting down
    if (m_board_data_manager_) {
        m_board_data_manager_->UnregisterNetIdChangeCallback();
        m_board_data_manager_->UnregisterSettingsChangeCallback();
        m_board_data_manager_->UnregisterLayerVisibilityChangeCallback();
    }

    if (m_render_pipeline_) {
        m_render_pipeline_->Shutdown();
        m_render_pipeline_.reset();
    }
    if (m_render_context_) {
        m_render_context_->Shutdown();
        m_render_context_.reset();
    }
    // std::cout << "PcbRenderer shutdown." << std::endl;
}

void PcbRenderer::Render(const Board* board, const Camera* camera, const Viewport* viewport, const Grid* grid)
{
    m_frame_rendered_this_cycle_ = false;  // Reset at the start of each Render call

    if (camera) {                                 // Check if camera is valid before using it
        if (camera->WasViewChangedThisFrame()) {  // This flag is true if camera moved this frame (before InteractionManager cleared it)
            if (m_interactive_frames_counter_ < kInteractiveThreshold) {
                m_interactive_frames_counter_++;
            }
            if (m_interactive_frames_counter_ >= kInteractiveThreshold && !m_is_interactive_optimized_) {
                if (m_render_context_)
                    m_render_context_->OptimizeForInteractive();
                m_is_interactive_optimized_ = true;
                // std::cout << "Switched to OptimizeForInteractive" << std::endl;
            }
        } else {
            if (m_interactive_frames_counter_ > 0) {
                m_interactive_frames_counter_ = 0;
            }
            if (m_is_interactive_optimized_) {
                if (m_render_context_)
                    m_render_context_->OptimizeForStatic();
                m_is_interactive_optimized_ = false;
                // std::cout << "Switched to OptimizeForStatic" << std::endl;
            }
        }
    } else {  // No camera, ensure static optimization and reset counters
        if (m_is_interactive_optimized_) {
            if (m_render_context_)
                m_render_context_->OptimizeForStatic();
            m_is_interactive_optimized_ = false;
        }
        m_interactive_frames_counter_ = 0;
    }

    if (!m_render_context_ || !m_render_pipeline_) {
        std::cerr << "PcbRenderer Error: Not initialized (context or pipeline missing)." << std::endl;
        return;
    }

    if (!camera || !viewport || !grid) {
        // Only redraw placeholder if needed or if it's the first time
        if (m_needs_redraw_signal_) {
            m_render_context_->BeginFrame();
            BLContext& ctx = m_render_context_->GetBlend2DContext();
            ctx.fillAll(BLRgba32(0xFF111111));
            std::cerr << "PcbRenderer::Render Warning: Missing critical components (camera, viewport, or grid). Rendering placeholder." << std::endl;
            if (!camera)
                std::cerr << "  - Camera is null" << std::endl;
            if (!viewport)
                std::cerr << "  - Viewport is null" << std::endl;
            if (!grid)
                std::cerr << "  - Grid is null" << std::endl;
            m_render_context_->EndFrame();
            m_frame_rendered_this_cycle_ = true;
            m_needs_redraw_signal_ = false;  //
        }
        return;
    }

    int viewportWidth = viewport->GetWidth();
    int viewportHeight = viewport->GetHeight();

    if (viewportWidth <= 0 || viewportHeight <= 0) {
        std::cerr << "PcbRenderer::Render Error: Invalid viewport dimensions (" << viewportWidth << "x" << viewportHeight << "). Skipping render." << std::endl;
        // Optionally, render a small placeholder or clear the existing image if it's differently sized
        if (m_needs_redraw_signal_ && m_render_context_->GetImageWidth() > 0 && m_render_context_->GetImageHeight() > 0) {  // Check if context has an image
            m_render_context_->BeginFrame();
            m_render_context_->GetBlend2DContext().fillAll(BLRgba32(0xFF000000));  // Fill black
            m_render_context_->EndFrame();
            m_frame_rendered_this_cycle_ = true;
            m_needs_redraw_signal_ = false;  // Consumed the redraw signal
        }
        return;
    }

    if (m_viewport_resized_signal_) {
        std::cout << "PcbRenderer::Render: Viewport size (" << viewportWidth << "x" << viewportHeight << ") differs from RenderContext image size (" << m_render_context_->GetImageWidth() << "x"
                  << m_render_context_->GetImageHeight() << "). Resizing context." << std::endl;
        if (!m_render_context_->ResizeImage(viewportWidth, viewportHeight)) {
            std::cerr << "PcbRenderer::Render Error: Failed to resize RenderContext image to match viewport. Skipping render." << std::endl;
            m_viewport_resized_signal_ = false;  // Acknowledge signal even on failure
            return;
        }
        m_full_redraw_needed_ = true;        // Force full redraw after resize
        m_viewport_resized_signal_ = false;  // Acknowledge signal
    }

    // If nothing needs to be redrawn, just return.
    if (!m_grid_dirty_ && !m_board_dirty_ && !m_full_redraw_needed_ && !m_needs_redraw_signal_) {
        return;
    }

    m_render_context_->BeginFrame();
    BLContext& bl_ctx = m_render_context_->GetBlend2DContext();

    try {
        // Pass the dirty flags to the pipeline execution
        m_render_pipeline_->BeginScene(bl_ctx);
        m_render_pipeline_->Execute(bl_ctx, board, *camera, *viewport, *grid, m_grid_dirty_ || m_full_redraw_needed_, m_board_dirty_ || m_full_redraw_needed_);
        m_render_pipeline_->EndScene();

        // Reset flags after successful rendering
        m_grid_dirty_ = false;
        m_board_dirty_ = false;
        m_full_redraw_needed_ = false;
        m_frame_rendered_this_cycle_ = true;
        m_needs_redraw_signal_ = false;  // Consumed the redraw signal
    } catch (const std::exception& e) {
        std::cerr << "PcbRenderer::Render Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "PcbRenderer::Render: Unknown exception during rendering" << std::endl;
    }

    m_render_context_->EndFrame();
}

const BLImage& PcbRenderer::GetRenderedImage() const
{
    if (!m_render_context_) {
        // This case should ideally not happen if used correctly (after init)
        // Return a static empty BLImage or throw an exception
        static BLImage emptyImage;
        std::cerr << "PcbRenderer Error: GetRenderedImage called when RenderContext is null." << std::endl;
        return emptyImage;
    }

    // Return the rendered image - no need for verbose logging every frame
    return m_render_context_->GetTargetImage();
}

void PcbRenderer::OnViewportResized(int newWidth, int newHeight)
{
    // This function is called by PCBViewerWindow when ImGui reports a content region size change.
    // We signal the Render() method to handle the actual resize and full redraw.
    if (m_render_context_) {
        if (m_render_context_->GetImageWidth() != newWidth || m_render_context_->GetImageHeight() != newHeight) {
            NotifyViewportResizedEvent();  // Signal that viewport resize is needed
                                           // The actual m_renderContext->ResizeImage will happen in the Render() method
                                           // based on m_viewportResizedSignal
        }
    } else {
        std::cerr << "PcbRenderer Warning: OnViewportResized called but RenderContext is not initialized." << std::endl;
    }
}

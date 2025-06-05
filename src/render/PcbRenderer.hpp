#pragma once

#include <memory>
#include <blend2d.h> // For BLImage

// Forward declarations
class RenderContext; // The Blend2D-focused RenderContext
class RenderPipeline;
class Board;
class Camera;
class Viewport;
class Grid;             // Added forward declaration for Grid
class BoardDataManager; // Added forward declaration

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
    bool Initialize(int initial_width, int initial_height, std::shared_ptr<BoardDataManager> board_data_manager);
    void Shutdown();

    // Main rendering method
    // Renders the PCB and grid to an internal BLImage managed by RenderContext.
    void Render(const Board *board, const Camera *camera, const Viewport *viewport, const Grid *grid); // Added Grid

    // Access the rendered image
    [[nodiscard]] const BLImage& GetRenderedImage() const;
    // Potentially a method to notify of viewport size changes to resize the BLImage
    void OnViewportResized(int new_width, int new_height);

    // Methods for conditional rendering
    void MarkGridDirty()
    {
        m_grid_dirty_ = true;
        m_needs_redraw_signal_ = true;
    }
    void MarkBoardDirty()
    {
        m_board_dirty_ = true;
        m_needs_redraw_signal_ = true;
    }
    void NotifyViewportResizedEvent()
    {
        m_viewport_resized_signal_ = true;
        m_needs_redraw_signal_ = true;
    } // Renamed to avoid conflict
    [[nodiscard]] bool WasFrameJustRendered() const { return m_frame_rendered_this_cycle_; }
    [[nodiscard]] bool NeedsRedraw() const { return m_needs_redraw_signal_; }

private:
    std::unique_ptr<RenderContext> m_render_context_;
    std::unique_ptr<RenderPipeline> m_render_pipeline_;
    std::shared_ptr<BoardDataManager> m_board_data_manager_;

    // Flags for conditional rendering
    bool m_grid_dirty_ = true;
    bool m_board_dirty_ = true;
    bool m_full_redraw_needed_ = true;          // True if viewport resize or initial render
    bool m_frame_rendered_this_cycle_ = false;  // True if Render() actually drew something
    bool m_viewport_resized_signal_ = false;    // Signal that viewport has been resized
    bool m_needs_redraw_signal_ = true;         // Master signal if any part needs redraw

    // For OptimizeForStatic/Interactive switching
    int m_interactive_frames_counter_ = 0;
    static const int kInteractiveThreshold = 2;  // Frames of continuous camera change
    bool m_is_interactive_optimized_ = false;
};
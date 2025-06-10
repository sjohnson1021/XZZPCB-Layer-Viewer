#pragma once

#include <string>  // Added for std::string
#include <unordered_map>
#include <memory>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <map>  // Added for std::map usage

#include <blend2d.h>  // Include for BLContext

#include "core/BoardDataManager.hpp"
#include "utils/Constants.hpp"  // For kPi
#include "pcb/elements/Element.hpp"  // For ElementType enum

// Forward declarations
class RenderContext;  // The Blend2D-focused RenderContext
class Board;          // Board is now central for layer info lookup
class Camera;
class Viewport;
class Grid;       // Forward declare Grid
class Trace;      // Forward declare Trace
class Via;        // Forward declare Via
class Arc;        // Forward declare Arc
class Component;  // Forward declare Component
class TextLabel;  // Forward declare TextLabel
class Pin;        // Forward declare Pin
// class Layer; // No longer forward-declaring the old Layer class assumption

// Performance optimization: Cached rendering state to avoid repeated BoardDataManager calls
struct RenderingState {
    int selected_net_id = -1;
    const class Element* selected_element = nullptr;
    BoardDataManager::BoardSide current_view_side = BoardDataManager::BoardSide::kBoth;
    float board_outline_thickness = 0.1f;
    std::unordered_map<BoardDataManager::ColorType, BLRgba32> theme_color_cache;
    std::unordered_map<int, BLRgba32> layer_id_color_cache;
    bool is_board_folding_enabled = false;

    // Cache validity tracking
    mutable bool is_valid = false;
    mutable std::shared_ptr<const Board> cached_board;
    mutable BoardDataManager::BoardSide cached_view_side = BoardDataManager::BoardSide::kBoth;
    mutable int cached_selected_net_id = -1;
    mutable const Element* cached_selected_element = nullptr;
};

// Performance optimization: Dirty region tracking for intelligent re-rendering
struct DirtyRegionTracker {
    bool full_redraw_needed = true;
    bool layer_visibility_changed = false;
    bool net_selection_changed = false;
    bool element_selection_changed = false;
    bool zoom_changed = false;
    bool pan_changed = false;
    bool theme_changed = false;

    // Viewport tracking
    BLRect last_viewport_rect = {0, 0, 0, 0};
    double last_zoom_level = 1.0;
    BLPoint last_pan_position = {0, 0};

    void MarkFullRedraw() {
        full_redraw_needed = true;
        layer_visibility_changed = false;
        net_selection_changed = false;
        element_selection_changed = false;
        zoom_changed = false;
        pan_changed = false;
        theme_changed = false;
    }

    void ClearFlags() {
        full_redraw_needed = false;
        layer_visibility_changed = false;
        net_selection_changed = false;
        element_selection_changed = false;
        zoom_changed = false;
        pan_changed = false;
        theme_changed = false;
    }

    bool NeedsRedraw() const {
        return full_redraw_needed || layer_visibility_changed || net_selection_changed ||
               element_selection_changed || zoom_changed || pan_changed || theme_changed;
    }
};

// Performance optimization: Cached board rendering
struct CachedBoardRender {
    BLImage cached_image;
    bool is_valid = false;
    BLRect cached_viewport;
    double cached_zoom;
    BLPoint cached_pan;
    std::shared_ptr<const Board> cached_board;
    int cached_selected_net_id = -1;
    const Element* cached_selected_element = nullptr;
    std::vector<bool> cached_layer_visibility;

    void Invalidate() {
        is_valid = false;
        cached_image.reset();
    }

    bool IsValidFor(const BLRect& viewport, double zoom, const BLPoint& pan,
                   std::shared_ptr<const Board> board, int selected_net_id,
                   const Element* selected_element, const std::vector<bool>& layer_visibility) const {
        return is_valid &&
               cached_board == board &&
               cached_selected_net_id == selected_net_id &&
               cached_selected_element == selected_element &&
               cached_layer_visibility == layer_visibility &&
               std::abs(cached_zoom - zoom) < 0.001 &&
               std::abs(cached_pan.x - pan.x) < 0.1 &&
               std::abs(cached_pan.y - pan.y) < 0.1 &&
               std::abs(cached_viewport.x - viewport.x) < 0.1 &&
               std::abs(cached_viewport.y - viewport.y) < 0.1 &&
               std::abs(cached_viewport.w - viewport.w) < 0.1 &&
               std::abs(cached_viewport.h - viewport.h) < 0.1;
    }
};

// Enhanced font caching structure
struct FontCacheKey {
    std::string font_family;
    float size;

    bool operator==(const FontCacheKey& other) const {
        return font_family == other.font_family && std::abs(size - other.size) < 0.01f;
    }
};

struct FontCacheKeyHash {
    std::size_t operator()(const FontCacheKey& key) const {
        return std::hash<std::string>{}(key.font_family) ^
               (std::hash<float>{}(key.size) << 1);
    }
};

// Work item for thread pool
struct RenderWorkItem {
    enum Type {
        TRACE_BATCH,
        COMPONENT_BATCH,
        PIN_BATCH,
        ARC_BATCH,
        VIA_BATCH,
        TEXT_BATCH
    };

    Type type;
    std::function<void()> work_function;

    RenderWorkItem(Type t, std::function<void()> func) : type(t), work_function(std::move(func)) {}
};

// Thread pool for persistent worker threads
class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();

    template<typename F>
    auto enqueue(F&& f) -> std::future<typename std::result_of<F()>::type> {
        using return_type = typename std::result_of<F()>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::forward<F>(f)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return res;
    }

    void wait_for_all();
    size_t get_thread_count() const { return workers_.size(); }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    std::atomic<size_t> active_tasks_;
    std::condition_variable completion_cv_;
};

// Multi-threading structures for parallel rendering
struct TraceRenderBatch {
    std::vector<const Trace*> traces;
    BLRgba32 color;
    double thickness;
    BLStrokeCap start_cap;
    BLStrokeCap end_cap;
    BLRect world_view_rect;

    TraceRenderBatch() = default;
    TraceRenderBatch(const BLRgba32& c, double t, BLStrokeCap sc, BLStrokeCap ec, const BLRect& rect)
        : color(c), thickness(t), start_cap(sc), end_cap(ec), world_view_rect(rect) {
        traces.reserve(1000); // Pre-allocate for performance
    }
};

struct ComponentRenderBatch {
    std::vector<const Component*> components;
    BLRect world_view_rect;
    std::unordered_map<BoardDataManager::ColorType, BLRgba32> theme_colors;
    int selected_net_id;
    const Element* selected_element;

    ComponentRenderBatch() : selected_net_id(-1), selected_element(nullptr) {
        components.reserve(100);
    }
};

class RenderPipeline
{
public:
    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;
    RenderPipeline(RenderPipeline&&) = delete;
    RenderPipeline& operator=(RenderPipeline&&) = delete;

    // Initialize with the Blend2D-focused RenderContext
    bool Initialize(RenderContext& context);
    void Shutdown();

    // Scene-level operations for the pipeline using a BLContext
    void BeginScene(BLContext& bl_ctx);  // Use BLContext directly
    void EndScene();                     // Might be a no-op for current plans

    // Main execution method to draw PCB elements and grid
    void Execute(BLContext& bl_ctx,
                 const Board* board,  // Changed from const Board& to allow nullptr
                 const Camera& camera,
                 const Viewport& viewport,
                 const Grid& grid,  // Pass Grid by const reference
                 bool render_grid,  // New: flag to control grid rendering
                 bool render_board  // New: flag to control board rendering
    );

    BLMatrix2D ViewMatrix(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport);

    // Cache invalidation for immediate color updates
    void InvalidateRenderingStateCache() const;



    // To keep PcbRenderer simple, RenderPipeline will be responsible for calling Grid::Render().
    void RenderGrid(BLContext& bl_ctx,
                    const Camera& camera,
                    const Viewport& viewport,
                    const Grid& grid  // Pass Grid by const reference
    );

    // Helper methods for rendering specific PCB elements
    void RenderTrace(BLContext& bl_ctx, const Trace& trace, const BLRect& world_view_rect, BLStrokeCap start_cap, BLStrokeCap end_cap, double thickness_override = -1.0);
    void RenderVia(BLContext& bl_ctx, const Via& via, const Board& board, const BLRect& world_view_rect, const BLRgba32& color_from, const BLRgba32& color_to);
    void RenderArc(BLContext& bl_ctx, const Arc& arc, const BLRect& world_view_rect, double thickness_override = -1.0);
    void RenderComponent(BLContext& bl_ctx,
                         const Component& component,
                         const Board& board,
                         const BLRect& world_view_rect,
                         const BLRgba32& component_fill_color,
						 const BLRgba32& component_stroke_color,
                         const std::unordered_map<BoardDataManager::ColorType, BLRgba32>& theme_color_cache,
                         int selected_net_id,
                         const class Element* selected_element = nullptr);
    void RenderTextLabel(BLContext& bl_ctx, const TextLabel& text_label, const BLRgba32& color);
    // TODO: Consider passing layer_properties_map to RenderTextLabel if it needs more than just color
    // void RenderPin(BLContext &bl_ctx, const Pin &pin, const Component &component, const Board &board, const BLRgba32 &highlightColor);
    void RenderPin(BLContext& ctx, const Pin& pin, const Component* parent_component, const BLRgba32& fill_color, const BLRgba32& stroke_color, const Board& board);

private:
    // Helper function to get the visible area in world coordinates
    [[nodiscard]] BLRect GetVisibleWorldBounds(const Camera& camera, const Viewport& viewport) const;

    // Performance optimization: Cached rendering state management
    const RenderingState& GetCachedRenderingState(const Board& board) const;

    // Performance optimization: Batch rendering methods to reduce state changes
    void SetFillColorOptimized(BLContext& ctx, const BLRgba32& color) const;
    void SetStrokeColorOptimized(BLContext& ctx, const BLRgba32& color) const;
    void SetStrokeWidthOptimized(BLContext& ctx, double width) const;
    void ResetBlend2DStateTracking() const;

    // Performance optimization: Object pool management
    BLPath& GetPooledPath() const;
    void ReturnPooledPath(BLPath& path) const;
    void InitializeObjectPools() const;

    // Performance monitoring and debugging
    void LogPerformanceStats() const;
    size_t GetElementsRendered() const { return m_elements_rendered_; }
    size_t GetElementsCulled() const { return m_elements_culled_; }
    double GetCullingRatio() const {
        size_t total = m_elements_rendered_ + m_elements_culled_;
        return total > 0 ? static_cast<double>(m_elements_culled_) / total : 0.0;
    }

    // Enhanced multi-threading methods for parallel rendering
    void InitializeThreadPool();
    void ShutdownThreadPool();



    void RenderComponentsOptimized(const std::vector<const Component*>& components,
                                  const Board& board,
                                  const BLRect& world_view_rect,
                                  const std::unordered_map<BoardDataManager::ColorType, BLRgba32>& theme_colors,
                                  int selected_net_id,
                                  const Element* selected_element);



    // Enhanced trace rendering with individual highlighting support
    void RenderTracesWithHighlighting(const std::vector<const Trace*>& traces,
                                     const BLRgba32& base_color,
                                     const BLRect& world_view_rect,
                                     BLStrokeCap start_cap,
                                     BLStrokeCap end_cap,
                                     double thickness_override,
                                     int selected_net_id,
                                     const Element* selected_element,
                                     const BLRgba32& highlight_color,
                                     const BLRgba32& selected_element_highlight_color);

    void RenderComponentsParallel(const std::vector<const Component*>& components,
                                 const Board& board,
                                 const BLRect& world_view_rect,
                                 const std::unordered_map<BoardDataManager::ColorType, BLRgba32>& theme_colors,
                                 int selected_net_id,
                                 const Element* selected_element);



    // Enhanced font management
    BLFont& GetCachedFont(const std::string& font_family, float size);
    void PreloadCommonFonts();

    // std::vector<std::unique_ptr<RenderStage>> m_stages;
    // Or a more direct approach if stages are fixed:
    // void GeometryPass(RenderContext& context);
    // void LightingPass(RenderContext& context);
    // void PostProcessingPass(RenderContext& context);
    // void UIPass(RenderContext& context);

    // Helper methods for drawing specific parts, called from Execute
    void RenderBoard(BLContext& bl_ctx, const Board& board, const Camera& camera, const Viewport& viewport, const BLRect& world_view_rect);

    RenderContext* m_render_context_ = nullptr;  // Store a pointer to the context if needed by multiple methods
    bool m_initialized_ = false;

    // Enhanced font caching system
    std::map<std::string, BLFontFace> m_font_face_cache_;
    std::unordered_map<FontCacheKey, BLFont, FontCacheKeyHash> m_font_cache_;
    mutable std::mutex m_font_cache_mutex_;

    // Performance optimization: Cached rendering state
    mutable RenderingState m_cached_rendering_state_;

    // Performance optimization: Batch rendering state
    mutable BLRgba32 m_last_fill_color_;
    mutable BLRgba32 m_last_stroke_color_;
    mutable double m_last_stroke_width_;
    mutable bool m_blend2d_state_dirty_;

    // Performance optimization: Culling statistics for debugging
    mutable size_t m_elements_rendered_;
    mutable size_t m_elements_culled_;



    // Performance optimization: Object pools for frequently allocated objects
    mutable std::vector<BLPath> m_path_pool_;
    mutable size_t m_path_pool_index_;

    // Performance optimization: Reusable containers to avoid allocations
    mutable std::vector<int> m_temp_layer_ids_;
    mutable std::vector<ElementType> m_temp_element_types_;

    // Enhanced multi-threading system with persistent thread pool
    mutable std::unique_ptr<ThreadPool> m_thread_pool_;
    mutable std::atomic<bool> m_threading_enabled_;
    mutable std::mutex m_thread_mutex_;
    mutable unsigned int m_thread_count_;

    // Performance tuning parameters
    mutable size_t m_min_traces_for_threading_;
    mutable size_t m_traces_per_thread_min_;
    mutable size_t m_min_components_for_threading_;
    mutable size_t m_components_per_thread_min_;

    // Add any other members needed for managing rendering state or resources for the pipeline
};

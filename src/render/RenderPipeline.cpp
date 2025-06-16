#include "render/RenderPipeline.hpp"

#include <algorithm>  // For std::min/max for AABB checks
#include <cmath>      // For std::cos and std::sin
#include <iomanip>    // For std::setprecision
#include <iostream>

#include <blend2d.h>

#include "pcb/Board.hpp"
#include "pcb/elements/Arc.hpp"        // Added for Arc element
#include "pcb/elements/Component.hpp"  // Added for Component
#include "pcb/elements/Pin.hpp"        // For Pin and PadShape when rendering pins
#include "pcb/elements/TextLabel.hpp"  // Added for TextLabel
#include "pcb/elements/Trace.hpp"      // Added for Trace element
#include "pcb/elements/Via.hpp"        // Added for Via element
#include "render/RenderContext.hpp"    // For access to RenderContext if needed
#include "view/Camera.hpp"
#include "view/Grid.hpp"  // For Grid class definition for RenderGrid
#include "view/Viewport.hpp"

// Default values for rendering elements
static constexpr double kDefaultTraceWidth = 0.05;
static constexpr double kMinViaExtent = 0.01;  // Used for AABB culling if radii are zero/negative
static constexpr double kDefaultArcThickness = 0.05;
static constexpr double kDefaultComponentMinDimension = 0.1;

// Layer ID constants
static constexpr int kSilkscreenLayerId = 17;
static constexpr int kBoardOutlineLayerId = 28;

// Forward declaration for use in RenderPin's lambda
static void RenderCapsule(BLContext& ctx, double width, double height, double x_coord, double y_coord, const BLRgba32& fill_color, const BLRgba32& stroke_color);

RenderPipeline::RenderPipeline() : m_render_context_(nullptr), m_initialized_(false),
    m_threading_enabled_(false), m_thread_count_(0),
    m_min_traces_for_threading_(100), m_traces_per_thread_min_(50),
    m_min_components_for_threading_(20), m_components_per_thread_min_(5)
{
    // Initialize threading based on hardware capabilities
    unsigned int hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads > 1) {
        // Use 75% of available threads, leaving some for system/other apps
        m_thread_count_ = std::max(1u, static_cast<unsigned int>(hardware_threads * 0.75));
        m_threading_enabled_ = true;

        // Adjust thresholds based on thread count for optimal performance - LOWERED for better utilization
        m_min_traces_for_threading_ = std::max(size_t(20), static_cast<size_t>(m_thread_count_) * 2);  // Much lower threshold
        m_traces_per_thread_min_ = std::max(size_t(5), m_min_traces_for_threading_ / static_cast<size_t>(m_thread_count_));

        // Component threading thresholds - LOWERED for better utilization
        m_min_components_for_threading_ = std::max(size_t(4), static_cast<size_t>(m_thread_count_) * 1);  // Much lower threshold
        m_components_per_thread_min_ = std::max(size_t(1), m_min_components_for_threading_ / static_cast<size_t>(m_thread_count_));

        std::cout << "RenderPipeline: Multi-threading enabled with " << m_thread_count_ << " threads" << std::endl;
        std::cout << "  Min traces for threading: " << m_min_traces_for_threading_ << std::endl;
        std::cout << "  Min traces per thread: " << m_traces_per_thread_min_ << std::endl;
        std::cout << "  Min components for threading: " << m_min_components_for_threading_ << std::endl;
        std::cout << "  Min components per thread: " << m_components_per_thread_min_ << std::endl;
        std::cout << "  Hardware threads detected: " << hardware_threads << std::endl;

        // Initialize thread pool immediately
        InitializeThreadPool();
    } else {
        std::cout << "RenderPipeline: Single-threaded mode (hardware_concurrency: " << hardware_threads << ")" << std::endl;
    }
}

RenderPipeline::~RenderPipeline()
{
    // Ensure Shutdown is called if not already.
    if (m_initialized_) {
        // std::cerr << "RenderPipeline destroyed without calling Shutdown() first!" << std::endl;
        Shutdown();
    }
    ShutdownThreadPool();
    // std::cout << "RenderPipeline destroyed." << std::endl;
}

bool RenderPipeline::Initialize(RenderContext& context)
{
    m_render_context_ = &context;  // Store pointer to the context

    // Initialize thread pool if threading is enabled
    if (m_threading_enabled_) {
        InitializeThreadPool();
    }

    // Initialize any pipeline-specific resources if needed.
    // For now, it mainly relies on the context passed during Execute.
    std::cout << "RenderPipeline initialized." << std::endl;
    m_initialized_ = true;
    return true;
}

void RenderPipeline::Shutdown()
{
    // Release any pipeline-specific resources.
    m_render_context_ = nullptr;
    // Release all pipeline resources, clear stages.
    // for (auto& stage : m_stages) {
    //     if (stage) stage->Shutdown();
    // }
    // m_stages.clear();
    m_font_face_cache_.clear();

    std::cout << "RenderPipeline shutdown." << std::endl;
    m_initialized_ = false;
}

void RenderPipeline::BeginScene(BLContext& bl_ctx)
{
    if (!m_initialized_)
        return;
    // This method is called by PcbRenderer.
    // It can be used to set up any per-scene state on the bl_ctx if necessary.
    // For now, PcbRenderer->BeginFrame() clears the target, which is sufficient.
    // bl_ctx.save(); // Example: save context state if pipeline makes many changes
    // std::cout << "RenderPipeline: Beginning scene." << std::endl;
}

void RenderPipeline::EndScene()
{
    if (!m_initialized_)
        return;
    // Called by PcbRenderer after Execute.
    // Can be used to restore context state if saved in BeginScene.
    // if (m_renderContext && m_renderContext->GetBlend2DContext().isActive()) { // Check if context is valid
    //     m_renderContext->GetBlend2DContext().restore(); // Example: restore context state
    // }
    // std::cout << "RenderPipeline: Ending scene." << std::endl;
}

void RenderPipeline::Execute(BLContext& bl_ctx,
                             const Board* board,
                             const Camera& camera,
                             const Viewport& viewport,
                             const Grid& grid,
                             bool render_grid,  // Parameter name matching declaration
                             bool render_board  // Parameter name matching declaration
)
{
    if (!m_initialized_) {
        std::cerr << "RenderPipeline::Execute Error: Not initialized." << std::endl;
        return;
    }

    // Conditionally render the grid
    if (render_grid) {
        RenderGrid(bl_ctx, camera, viewport, grid);
    }

    // Conditionally render the board
    if (render_board && board) {
        BLRect world_view_rect = GetVisibleWorldBounds(camera, viewport);  // Calculate once
        RenderBoard(bl_ctx, *board, camera, viewport, world_view_rect);    // Pass to RenderBoard
    }

    // Grid measurement overlay is now rendered by Application layer using ImGui
}

/**
 * @brief Creates a view transformation matrix for rendering.
 * @param bl_ctx The Blend2D context
 * @param camera The camera containing position, zoom, and rotation
 * @param viewport The viewport containing screen dimensions
 * @return The transformation matrix for world-to-screen conversion
 */
BLMatrix2D RenderPipeline::ViewMatrix(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport)
{
    BLMatrix2D view_matrix = bl_ctx.metaTransform();
    view_matrix.translate(viewport.GetWidth() / 2.0, viewport.GetHeight() / 2.0);
    view_matrix.scale(camera.GetZoom());
    view_matrix.rotate(-camera.GetRotation() * (static_cast<float>(kPi) / 180.0f));
    view_matrix.translate(-camera.GetPosition().x_ax, -camera.GetPosition().y_ax);
    return view_matrix;
}

/**
 * @brief Checks if two axis-aligned bounding boxes intersect.
 * @param r1 First rectangle
 * @param r2 Second rectangle
 * @return true if rectangles intersect, false otherwise
 */
static bool AreRectsIntersecting(const BLRect& r1, const BLRect& r2)
{
    double r1_w = std::max(0.0, r1.w);
    double r1_h = std::max(0.0, r1.h);
    double r2_w = std::max(0.0, r2.w);
    double r2_h = std::max(0.0, r2.h);
    double r1_x2 = r1.x + r1_w;
    double r1_y2 = r1.y + r1_h;
    double r2_x2 = r2.x + r2_w;
    double r2_y2 = r2.y + r2_h;
    return !(r1_x2 < r2.x || r1.x > r2_x2 || r1_y2 < r2.y || r1.y > r2_y2);
}
static bool ArePointsClose(const BLPoint& p1, const BLPoint& p2, double epsilon = 1e-6)
{
    return std::abs(p1.x - p2.x) < epsilon && std::abs(p1.y - p2.y) < epsilon;
}

/**
 * @brief Transforms an axis-aligned bounding box by a transformation matrix.
 * @param local_aabb The local bounding box to transform
 * @param transform The transformation matrix to apply
 * @return The transformed bounding box
 */
BLRect TransformAABB(const BLRect& local_aabb, const BLMatrix2D& transform)
{
    BLPoint p1 = {local_aabb.x, local_aabb.y};
    BLPoint p2 = {local_aabb.x + local_aabb.w, local_aabb.y};
    BLPoint p3 = {local_aabb.x, local_aabb.y + local_aabb.h};
    BLPoint p4 = {local_aabb.x + local_aabb.w, local_aabb.y + local_aabb.h};

    BLPoint tp1 = transform.mapPoint(p1);
    BLPoint tp2 = transform.mapPoint(p2);
    BLPoint tp3 = transform.mapPoint(p3);
    BLPoint tp4 = transform.mapPoint(p4);

    double min_x = std::min({tp1.x, tp2.x, tp3.x, tp4.x});
    double min_y = std::min({tp1.y, tp2.y, tp3.y, tp4.y});
    double max_x = std::max({tp1.x, tp2.x, tp3.x, tp4.x});
    double max_y = std::max({tp1.y, tp2.y, tp3.y, tp4.y});

    return BLRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

BLRect RenderPipeline::GetVisibleWorldBounds(const Camera& camera, const Viewport& viewport) const
{
    // This logic is similar to Grid::GetVisibleWorldBounds
    Vec2 screen_corners[4] = {{static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY())},
                              {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY())},
                              {static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY() + viewport.GetHeight())},
                              {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY() + viewport.GetHeight())}};

    Vec2 world_min = viewport.ScreenToWorld(screen_corners[0], camera);
    Vec2 world_max = world_min;

    for (int i = 1; i < 4; ++i) {
        Vec2 world_corner = viewport.ScreenToWorld(screen_corners[i], camera);
        world_min.x_ax = std::min(world_min.x_ax, world_corner.x_ax);
        world_min.y_ax = std::min(world_min.y_ax, world_corner.y_ax);
        world_max.x_ax = std::max(world_max.x_ax, world_corner.x_ax);
        world_max.y_ax = std::max(world_max.y_ax, world_corner.y_ax);
    }
    // world_min and world_max are now correctly in Y-Down world coordinates.
    // BLRect expects (x, y, width, height) where x,y is the top-left corner.
    // In Y-Down, the top-left y is world_min.y.
    return BLRect(world_min.x_ax, world_min.y_ax, world_max.x_ax - world_min.x_ax, world_max.y_ax - world_min.y_ax);
}

void RenderPipeline::RenderBoard(BLContext& bl_ctx, const Board& board, const Camera& camera, const Viewport& viewport, const BLRect& world_view_rect)
{
    // Performance optimization: Reset state tracking for this frame
    ResetBlend2DStateTracking();

    bl_ctx.save();
    bl_ctx.applyTransform(ViewMatrix(bl_ctx, camera, viewport));

    // Performance optimization: Use cached rendering state to avoid repeated BoardDataManager calls
    const RenderingState& render_state = GetCachedRenderingState(board);

    // Use cached values instead of repeated function calls
    const int selected_net_id = render_state.selected_net_id;
    const Element* selected_element = render_state.selected_element;
    const BoardDataManager::BoardSide current_view_side = render_state.current_view_side;
    const float board_outline_thickness = render_state.board_outline_thickness;
    const auto& theme_color_cache = render_state.theme_color_cache;
    const auto& layer_id_color_cache = render_state.layer_id_color_cache;

    // Visual mirror transformation removed - actual element coordinates are now updated
    // when board flip state changes, so no runtime visual transformation is needed
    const BLRect adjusted_world_view_rect = world_view_rect;

    // Performance optimization: Pre-compute fallback colors to avoid repeated map lookups
    const BLRgba32 fallback_color(0xFFFF0000);  // Red
    const BLRgba32 highlight_color = theme_color_cache.count(BoardDataManager::ColorType::kNetHighlight) ?
        theme_color_cache.at(BoardDataManager::ColorType::kNetHighlight) : BLRgba32(0xFFFFFF00);
    const BLRgba32 selected_element_highlight_color = theme_color_cache.count(BoardDataManager::ColorType::kSelectedElementHighlight) ?
        theme_color_cache.at(BoardDataManager::ColorType::kSelectedElementHighlight) : BLRgba32(0xFFFFFF00);
    const BLRgba32 component_fill_color = theme_color_cache.count(BoardDataManager::ColorType::kComponentFill) ?
        theme_color_cache.at(BoardDataManager::ColorType::kComponentFill) : BLRgba32(0xFF007BFF);
    const BLRgba32 component_stroke_color = theme_color_cache.count(BoardDataManager::ColorType::kComponentStroke) ?
        theme_color_cache.at(BoardDataManager::ColorType::kComponentStroke) : BLRgba32(0xFF000000);
    const BLRgba32 pin_fill_color = theme_color_cache.count(BoardDataManager::ColorType::kPinFill) ?
        theme_color_cache.at(BoardDataManager::ColorType::kPinFill) : BLRgba32(0xC0999999);
    const BLRgba32 pin_stroke_color = theme_color_cache.count(BoardDataManager::ColorType::kPinStroke) ?
        theme_color_cache.at(BoardDataManager::ColorType::kPinStroke) : BLRgba32(0xC0000000);
    const BLRgba32 base_layer_theme_color = theme_color_cache.count(BoardDataManager::ColorType::kBaseLayer) ?
        theme_color_cache.at(BoardDataManager::ColorType::kBaseLayer) : fallback_color;
    const BLRgba32 silkscreen_theme_color = theme_color_cache.count(BoardDataManager::ColorType::kSilkscreen) ?
        theme_color_cache.at(BoardDataManager::ColorType::kSilkscreen) : BLRgba32(0xFFFFFFFF);
    const BLRgba32 board_edges_theme_color = theme_color_cache.count(BoardDataManager::ColorType::kBoardEdges) ?
        theme_color_cache.at(BoardDataManager::ColorType::kBoardEdges) : BLRgba32(0xFF00FF00);

    // Performance optimization: Create optimized render pass lambda with reduced overhead
    auto executeRenderPass = [&](const std::vector<int>& target_layer_ids,
                                 const std::vector<ElementType>& target_element_types,
                                 std::map<int, std::pair<BLPoint, BLPoint>>& trace_cap_manager,
                                 bool is_silkscreen_pass = false,
                                 bool is_board_outline_pass = false) {

        // Performance optimization: Collect traces for parallel processing
        std::vector<const Trace*> traces_to_render;
        traces_to_render.reserve(1000); // Pre-allocate for performance

        // Performance optimization: Direct layer access instead of iterating all layers
        for (int layer_id : target_layer_ids) {
            auto layer_elements_it = board.m_elements_by_layer.find(layer_id);
            if (layer_elements_it == board.m_elements_by_layer.end()) {
                continue;
            }

            // Performance optimization: Check layer visibility once per layer
            const Board::LayerInfo* layer_info = board.GetLayerById(layer_id);
            if (!layer_info || !layer_info->IsVisible()) {
                continue;
            }

            const auto& elements_on_layer = layer_elements_it->second;

            // Performance optimization: Reserve space for element processing if needed
            // and batch similar operations together
            for (const auto& element_ptr : elements_on_layer) {
                if (!element_ptr || !element_ptr->IsVisible())
                    continue;

                const ElementType current_type = element_ptr->GetElementType();

                // Performance optimization: Use early exit for type matching
                if (!target_element_types.empty()) {
                    bool type_matches = false;
                    for (ElementType target_type : target_element_types) {
                        if (current_type == target_type) {
                            type_matches = true;
                            break;
                        }
                    }
                    if (!type_matches) {
                        continue;
                    }
                }

                // Board side filtering for silkscreen elements when board folding is enabled
                if (is_silkscreen_pass && current_view_side != BoardDataManager::BoardSide::kBoth) {
                    // Only apply side filtering to silkscreen elements that have board side assigned
                    if (element_ptr->HasBoardSideAssigned()) {
                        MountingSide element_side = element_ptr->GetBoardSide();
                        if ((current_view_side == BoardDataManager::BoardSide::kTop && element_side != MountingSide::kTop) ||
                            (current_view_side == BoardDataManager::BoardSide::kBottom && element_side != MountingSide::kBottom)) {
                            continue; // Skip this element as it's on the wrong side
                        }
                    }
                }

                bool is_selected_net = (selected_net_id != -1 && element_ptr->GetNetId() == selected_net_id);
                bool is_selected_element = (selected_element != nullptr && element_ptr.get() == selected_element);
                BLRgba32 current_element_color;

                if (is_selected_element) {
                    // Selected element takes priority over net highlighting
                    current_element_color = selected_element_highlight_color;
                } else if (is_selected_net) {
                    current_element_color = highlight_color;
                } else if (current_type == ElementType::kComponent) {  // Components handled separately unless forced here
                    current_element_color = component_fill_color;
                } else if (current_type == ElementType::kPin) {  // Standalone pins
                    current_element_color = pin_fill_color;
                } else if (is_silkscreen_pass &&
                           (current_type == ElementType::kTextLabel || current_type == ElementType::kArc || current_type == ElementType::kTrace)) {  // Example: Text, Arcs, Traces on silkscreen
                    current_element_color = silkscreen_theme_color;
                } else if (is_board_outline_pass) {  // Example: Board outlines
                    current_element_color = board_edges_theme_color;
                } else {  // Generic elements (Traces, Arcs, Vias, non-silkscreen Text)
                    current_element_color = layer_id_color_cache.count(layer_id) ? layer_id_color_cache.at(layer_id) : base_layer_theme_color;
                }

                bl_ctx.setStrokeStyle(current_element_color);
                bl_ctx.setFillStyle(current_element_color);

                switch (current_type) {
                    case ElementType::kTrace:
                        if (auto trace = dynamic_cast<const Trace*>(element_ptr.get())) {
                            // Collect traces for parallel processing instead of rendering immediately
                            traces_to_render.push_back(trace);

                            // Still need to update trace cap manager for proper cap rendering
                            int net_id = trace->GetNetId();
                            BLPoint current_start_point(trace->GetStartX(), trace->GetStartY());
                            BLPoint current_end_point(trace->GetEndX(), trace->GetEndY());
                            trace_cap_manager[net_id] = {current_start_point, current_end_point};
                        }
                        break;
                    case ElementType::kArc:
                        if (auto arc = dynamic_cast<const Arc*>(element_ptr.get())) {
                            // Pass board outline thickness for board outline elements
                            double thickness_override = is_board_outline_pass ? board_outline_thickness : -1.0;
                            RenderArc(bl_ctx, *arc, adjusted_world_view_rect, thickness_override);
                        }
                        break;
                    case ElementType::kVia:
                        if (auto via = dynamic_cast<const Via*>(element_ptr.get())) {
                            BLRgba32 via_color_from;
                            BLRgba32 via_color_to;
                            if (is_selected_element) {
                                // Selected element takes priority
                                via_color_from = selected_element_highlight_color;
                                via_color_to = selected_element_highlight_color;
                            } else if (is_selected_net) {
                                via_color_from = highlight_color;
                                via_color_to = highlight_color;
                            } else {
                                via_color_from = layer_id_color_cache.count(via->GetLayerFrom()) ? layer_id_color_cache.at(via->GetLayerFrom()) : base_layer_theme_color;
                                via_color_to = layer_id_color_cache.count(via->GetLayerTo()) ? layer_id_color_cache.at(via->GetLayerTo()) : base_layer_theme_color;
                            }
                            RenderVia(bl_ctx, *via, board, adjusted_world_view_rect, via_color_from, via_color_to);
                        }
                        break;
                    case ElementType::kComponent:
                        // Components are rendered in their own dedicated pass to ensure correct order and specific handling.
                        // This case in the general lambda might be skipped or only for very specific scenarios.
                        break;
                    case ElementType::kPin:  // For standalone pins not part of a component drawn via RenderComponent
                        if (auto pin = dynamic_cast<const Pin*>(element_ptr.get())) {
                            // A pin element here implies it's not part of a kCompLayer component.
                            // It needs a parent_component for GetPinWorldTransform if its coords are local.
                            // Assuming standalone pins have world coords or GetPinWorldTransform handles nullptr parent.
                            // RenderPin(bl_ctx, *pin, nullptr, current_element_color);
                        }
                        break;
                    case ElementType::kTextLabel:
                        // Text label rendering disabled - using ImGui tooltips instead
                        break;
                    default:
                        break;
                }
            }
        }

        // Performance optimization: Render all collected traces with individual highlighting support
        if (!traces_to_render.empty()) {
            // Determine base color for this batch of traces
            BLRgba32 base_trace_color;
            if (is_silkscreen_pass) {
                base_trace_color = silkscreen_theme_color;
            } else if (is_board_outline_pass) {
                base_trace_color = board_edges_theme_color;
            } else {
                // Use the first layer's color for this batch
                int first_layer_id = target_layer_ids.empty() ? 1 : target_layer_ids[0];
                base_trace_color = layer_id_color_cache.count(first_layer_id) ?
                                  layer_id_color_cache.at(first_layer_id) : base_layer_theme_color;
            }

            // Determine thickness override
            double thickness_override = is_board_outline_pass ? board_outline_thickness : -1.0;

            // Render traces with individual highlighting support
            RenderTracesWithHighlighting(traces_to_render, base_trace_color, adjusted_world_view_rect,
                                       BL_STROKE_CAP_ROUND, BL_STROKE_CAP_ROUND, thickness_override,
                                       selected_net_id, selected_element, highlight_color, selected_element_highlight_color);
        }
    };

    // --- Rendering Passes ---
    // Implement view perspective-based rendering order for realistic depth perception

    // Performance optimization: Get populated trace layers from cached rendering state
    // This avoids repeated iteration over all trace layers
    std::vector<int> populated_trace_layers;
    populated_trace_layers.reserve(16); // Pre-allocate for maximum possible trace layers

    for (int layer_id = Board::kTraceLayersStart; layer_id <= Board::kTraceLayersEnd; ++layer_id) {
        auto layer_it = board.m_elements_by_layer.find(layer_id);
        if (layer_it != board.m_elements_by_layer.end() && !layer_it->second.empty()) {
            populated_trace_layers.push_back(layer_id);
        }
    }

    // Performance optimization: Use cached board folding state
    const bool is_board_folding_enabled = render_state.is_board_folding_enabled;

    // Performance optimization: Pre-allocate rendering order vector
    std::vector<int> rendering_order;
    rendering_order.reserve(populated_trace_layers.size() + 4); // trace layers + 4 component/pin layers

    if (current_view_side == BoardDataManager::BoardSide::kBottom) {
        // Bottom-Up View (viewing from below): 31, 30 → trace layers 16→1 → 0, -1
        rendering_order.push_back(Board::kBottomPinsLayer);    // Layer 31
        rendering_order.push_back(Board::kBottomCompLayer);    // Layer 30

        // Add trace layers in descending order (16→1)
        if (is_board_folding_enabled) {
            // When folding is enabled, render all populated trace layers in reverse order
            for (auto it = populated_trace_layers.rbegin(); it != populated_trace_layers.rend(); ++it) {
                rendering_order.push_back(*it);
            }
        } else {
            // When folding is disabled, trace layers are split between sides
            // For bottom-up view, render the "bottom half" first, then "top half"
            int total_layers = static_cast<int>(populated_trace_layers.size());
            int split_point = total_layers / 2;

            // Bottom half (higher numbered layers) in descending order
            for (int i = total_layers - 1; i >= split_point; --i) {
                rendering_order.push_back(populated_trace_layers[i]);
            }
            // Top half (lower numbered layers) in descending order
            for (int i = split_point - 1; i >= 0; --i) {
                rendering_order.push_back(populated_trace_layers[i]);
            }
        }

        rendering_order.push_back(Board::kTopCompLayer);       // Layer 0
        rendering_order.push_back(Board::kTopPinsLayer);       // Layer -1

    } else {
        // Top-Down View (viewing from above): -1, 0 → trace layers 1→16 → 30, 31
        rendering_order.push_back(Board::kTopPinsLayer);       // Layer -1
        rendering_order.push_back(Board::kTopCompLayer);       // Layer 0

        // Add trace layers in ascending order (1→16)
        if (is_board_folding_enabled) {
            // When folding is enabled, render all populated trace layers in normal order
            for (int layer_id : populated_trace_layers) {
                rendering_order.push_back(layer_id);
            }
        } else {
            // When folding is disabled, trace layers are split between sides
            // For top-down view, render the "top half" first, then "bottom half"
            int total_layers = static_cast<int>(populated_trace_layers.size());
            int split_point = total_layers / 2;

            // Top half (lower numbered layers) in ascending order
            for (int i = 0; i < split_point; ++i) {
                rendering_order.push_back(populated_trace_layers[i]);
            }
            // Bottom half (higher numbered layers) in ascending order
            for (int i = split_point; i < total_layers; ++i) {
                rendering_order.push_back(populated_trace_layers[i]);
            }
        }

        rendering_order.push_back(Board::kBottomCompLayer);    // Layer 30
        rendering_order.push_back(Board::kBottomPinsLayer);    // Layer 31
    }

    // Performance optimization: Execute rendering passes with reduced overhead
    std::map<int, std::pair<BLPoint, BLPoint>> trace_cap_manager_copper;

    // Performance optimization: Use reusable containers
    m_temp_layer_ids_.clear();
    m_temp_element_types_.clear();
    m_temp_element_types_.reserve(3);
    m_temp_element_types_.push_back(ElementType::kTrace);
    m_temp_element_types_.push_back(ElementType::kArc);
    m_temp_element_types_.push_back(ElementType::kVia);

    for (int layer_id : rendering_order) {
        if (layer_id >= Board::kTraceLayersStart && layer_id <= Board::kTraceLayersEnd) {
            // Trace layers (1-16) - use reusable containers
            m_temp_layer_ids_.clear();
            m_temp_layer_ids_.push_back(layer_id);
            executeRenderPass(m_temp_layer_ids_, m_temp_element_types_, trace_cap_manager_copper);
        } else if (layer_id == Board::kTopCompLayer || layer_id == Board::kBottomCompLayer) {
            // Component layers (0, 30) - handled by existing component rendering logic below
            // Skip here as components are rendered in their own dedicated pass
        } else if (layer_id == Board::kTopPinsLayer || layer_id == Board::kBottomPinsLayer) {
            // Pin layers (-1, 31) - pins are rendered as part of their parent components
            // Skip here as pins are rendered within the component rendering pass
        }
    }

    // Render other layers (silkscreen, unknown layers, board outline) after main layers
    std::map<int, std::pair<BLPoint, BLPoint>> trace_cap_manager_silkscreen;
    executeRenderPass({kSilkscreenLayerId}, {} /* all types */, trace_cap_manager_silkscreen, true /*is_silkscreen_pass*/);

    std::vector<int> other_trace_layer_ids;
    for (int i = 18; i <= 27; ++i)
        other_trace_layer_ids.push_back(i);
    std::map<int, std::pair<BLPoint, BLPoint>> trace_cap_manager_other;
    executeRenderPass(other_trace_layer_ids, {ElementType::kTrace, ElementType::kArc, ElementType::kVia}, trace_cap_manager_other);

    std::map<int, std::pair<BLPoint, BLPoint>> trace_cap_manager_board_outline;
    executeRenderPass({kBoardOutlineLayerId}, {} /* all types */, trace_cap_manager_board_outline, false, true /*is_board_outline_pass*/);

    // Performance optimization: Render components in proper depth order
    // Extract component layers from the rendering order to maintain proper depth
    std::vector<int> component_layer_ids;
    component_layer_ids.reserve(2); // At most 2 component layers

    for (int layer_id : rendering_order) {
        if (layer_id == Board::kTopCompLayer || layer_id == Board::kBottomCompLayer) {
            component_layer_ids.push_back(layer_id);
        }
    }

    // Performance optimization: Collect all components for parallel processing
    std::vector<const Component*> all_components;
    all_components.reserve(200); // Pre-allocate for typical component count

    for (int comp_layer_id : component_layer_ids) {
        auto comp_layer_elements_it = board.m_elements_by_layer.find(comp_layer_id);
        if (comp_layer_elements_it == board.m_elements_by_layer.end()) {
            continue;  // This layer doesn't exist or has no elements
        }

        const auto& elements_on_comp_layer = comp_layer_elements_it->second;

        // Performance optimization: Pre-check layer visibility once per layer
        const Board::LayerInfo* comp_layer_info = board.GetLayerById(comp_layer_id);
        if (!comp_layer_info || !comp_layer_info->IsVisible()) {
            continue; // Skip entire layer if not visible
        }

        for (const auto& element_ptr : elements_on_comp_layer) {
            if (!element_ptr || element_ptr->GetElementType() != ElementType::kComponent || !element_ptr->IsVisible()) {
                continue;
            }

            const Component* component_to_render = dynamic_cast<const Component*>(element_ptr.get());
            if (!component_to_render) {
                continue;
            }

            // Performance optimization: Early exit for view side filtering
            if (current_view_side != BoardDataManager::BoardSide::kBoth) {
                if ((current_view_side == BoardDataManager::BoardSide::kTop && component_to_render->side != MountingSide::kTop) ||
                    (current_view_side == BoardDataManager::BoardSide::kBottom && component_to_render->side != MountingSide::kBottom)) {
                    continue;
                }
            }

            all_components.push_back(component_to_render);
        }
    }

    // Render all components using parallel processing
    if (!all_components.empty()) {
        RenderComponentsOptimized(all_components, board, adjusted_world_view_rect, theme_color_cache, selected_net_id, selected_element);
    }
    bl_ctx.restore();
}

// Performance optimization: Cached rendering state management
const RenderingState& RenderPipeline::GetCachedRenderingState(const Board& board) const
{
    // Check if cache is still valid
    if (m_cached_rendering_state_.is_valid &&
        m_render_context_ && m_render_context_->GetBoardDataManager()) {

        auto bdm = m_render_context_->GetBoardDataManager();
        std::shared_ptr<const Board> current_board = bdm->GetBoard();

        // Quick validation of cache validity
        if (current_board == m_cached_rendering_state_.cached_board &&
            bdm->GetCurrentViewSide() == m_cached_rendering_state_.cached_view_side &&
            bdm->GetSelectedNetId() == m_cached_rendering_state_.cached_selected_net_id &&
            bdm->GetSelectedElement() == m_cached_rendering_state_.cached_selected_element) {
            return m_cached_rendering_state_;
        }
    }

    // Cache is invalid, rebuild it
    m_cached_rendering_state_.is_valid = false;

    if (m_render_context_ && m_render_context_->GetBoardDataManager()) {
        auto bdm = m_render_context_->GetBoardDataManager();

        // Cache basic state
        m_cached_rendering_state_.selected_net_id = bdm->GetSelectedNetId();
        m_cached_rendering_state_.selected_element = bdm->GetSelectedElement();
        m_cached_rendering_state_.current_view_side = bdm->GetCurrentViewSide();
        m_cached_rendering_state_.board_outline_thickness = bdm->GetBoardOutlineThickness();
        m_cached_rendering_state_.is_board_folding_enabled = bdm->IsBoardFoldingEnabled();

        // Cache theme colors (batch operation to reduce function call overhead)
        auto& theme_cache = m_cached_rendering_state_.theme_color_cache;
        theme_cache.clear();
        theme_cache.reserve(10); // Pre-allocate for known color types

        theme_cache[BoardDataManager::ColorType::kNetHighlight] = bdm->GetColor(BoardDataManager::ColorType::kNetHighlight);
        theme_cache[BoardDataManager::ColorType::kSelectedElementHighlight] = bdm->GetColor(BoardDataManager::ColorType::kSelectedElementHighlight);
        theme_cache[BoardDataManager::ColorType::kComponentFill] = bdm->GetColor(BoardDataManager::ColorType::kComponentFill);
        theme_cache[BoardDataManager::ColorType::kComponentStroke] = bdm->GetColor(BoardDataManager::ColorType::kComponentStroke);
        theme_cache[BoardDataManager::ColorType::kPinFill] = bdm->GetColor(BoardDataManager::ColorType::kPinFill);
        theme_cache[BoardDataManager::ColorType::kPinStroke] = bdm->GetColor(BoardDataManager::ColorType::kPinStroke);
        theme_cache[BoardDataManager::ColorType::kBaseLayer] = bdm->GetColor(BoardDataManager::ColorType::kBaseLayer);
        theme_cache[BoardDataManager::ColorType::kSilkscreen] = bdm->GetColor(BoardDataManager::ColorType::kSilkscreen);
        theme_cache[BoardDataManager::ColorType::kBoardEdges] = bdm->GetColor(BoardDataManager::ColorType::kBoardEdges);

        // Cache layer colors (batch operation)
        auto& layer_cache = m_cached_rendering_state_.layer_id_color_cache;
        layer_cache.clear();

        const auto& board_layers = board.GetLayers();
        layer_cache.reserve(board_layers.size()); // Pre-allocate

        for (const Board::LayerInfo& layer_info_entry : board_layers) {
            layer_cache[layer_info_entry.GetId()] = bdm->GetLayerColor(layer_info_entry.GetId());
        }

        // Update cache validity tracking
        m_cached_rendering_state_.cached_board = bdm->GetBoard();
        m_cached_rendering_state_.cached_view_side = m_cached_rendering_state_.current_view_side;
        m_cached_rendering_state_.cached_selected_net_id = m_cached_rendering_state_.selected_net_id;
        m_cached_rendering_state_.cached_selected_element = m_cached_rendering_state_.selected_element;
        m_cached_rendering_state_.is_valid = true;
    }

    return m_cached_rendering_state_;
}

void RenderPipeline::InvalidateRenderingStateCache() const
{
    m_cached_rendering_state_.is_valid = false;
}

// Performance optimization: Batch rendering methods to reduce Blend2D state changes
void RenderPipeline::SetFillColorOptimized(BLContext& ctx, const BLRgba32& color) const
{
    if (m_blend2d_state_dirty_ || m_last_fill_color_.value != color.value) {
        ctx.setFillStyle(color);
        m_last_fill_color_ = color;
    }
}

void RenderPipeline::SetStrokeColorOptimized(BLContext& ctx, const BLRgba32& color) const
{
    if (m_blend2d_state_dirty_ || m_last_stroke_color_.value != color.value) {
        ctx.setStrokeStyle(color);
        m_last_stroke_color_ = color;
    }
}

void RenderPipeline::SetStrokeWidthOptimized(BLContext& ctx, double width) const
{
    if (m_blend2d_state_dirty_ || std::abs(m_last_stroke_width_ - width) > 0.001) {
        ctx.setStrokeWidth(width);
        m_last_stroke_width_ = width;
    }
}

void RenderPipeline::ResetBlend2DStateTracking() const
{
    m_blend2d_state_dirty_ = true;
    m_elements_rendered_ = 0;
    m_elements_culled_ = 0;
}

// Performance optimization: Object pool management
BLPath& RenderPipeline::GetPooledPath() const
{
    if (m_path_pool_.empty()) {
        InitializeObjectPools();
    }

    if (m_path_pool_index_ >= m_path_pool_.size()) {
        m_path_pool_index_ = 0;
    }

    BLPath& path = m_path_pool_[m_path_pool_index_++];
    path.clear();  // Reset the path for reuse
    return path;
}

void RenderPipeline::ReturnPooledPath(BLPath& path) const
{
    // Path is automatically returned to pool when GetPooledPath cycles
    // This method exists for future expansion if needed
}

void RenderPipeline::InitializeObjectPools() const
{
    // Initialize path pool with reasonable size
    m_path_pool_.reserve(50);
    for (int i = 0; i < 50; ++i) {
        m_path_pool_.emplace_back();
    }
    m_path_pool_index_ = 0;

    // Initialize reusable containers
    m_temp_layer_ids_.reserve(20);
    m_temp_element_types_.reserve(10);
}

// Performance monitoring and debugging
void RenderPipeline::LogPerformanceStats() const
{
    #ifdef DEBUG_PERFORMANCE
    size_t total_elements = m_elements_rendered_ + m_elements_culled_;
    if (total_elements > 0) {
        double culling_ratio = static_cast<double>(m_elements_culled_) / total_elements * 100.0;
        std::cout << "RenderPipeline Performance Stats:" << std::endl;
        std::cout << "  Elements Rendered: " << m_elements_rendered_ << std::endl;
        std::cout << "  Elements Culled: " << m_elements_culled_ << std::endl;
        std::cout << "  Culling Ratio: " << std::fixed << std::setprecision(1) << culling_ratio << "%" << std::endl;
        std::cout << "  Cache Valid: " << (m_cached_rendering_state_.is_valid ? "Yes" : "No") << std::endl;
    }
    #endif
}

void RenderPipeline::RenderGrid(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, const Grid& grid)
{
    try {
        // Save Blend2D context state before grid rendering
        bl_ctx.save();

        // Call the Grid's own render method, providing the Blend2D context.
        grid.Render(bl_ctx, camera, viewport);

        // Restore Blend2D context state after grid rendering
        bl_ctx.restore();
    } catch (const std::exception& e) {
        std::cerr << "RenderPipeline::RenderGrid Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "RenderPipeline::RenderGrid: Unknown exception during grid rendering" << std::endl;
    }
}

// Private helper methods for rendering specific PCB elements
// These would typically be declared in RenderPipeline.hpp if it were being modified here.

void RenderPipeline::RenderTrace(BLContext& bl_ctx, const Trace& trace, const BLRect& world_view_rect, BLStrokeCap start_cap, BLStrokeCap end_cap, double thickness_override)
{
    // Performance optimization: Cache trace coordinates to avoid repeated function calls
    const double start_x = trace.GetStartX();
    const double start_y = trace.GetStartY();
    const double end_x = trace.GetEndX();
    const double end_y = trace.GetEndY();

    // Performance optimization: Optimized bounding box calculation
    const double min_x = std::min(start_x, end_x);
    const double max_x = std::max(start_x, end_x);
    const double min_y = std::min(start_y, end_y);
    const double max_y = std::max(start_y, end_y);

    const double trace_width = trace.GetWidth();
    const double width_for_bounds = trace_width > 0 ? trace_width : kDefaultTraceWidth;
    const double half_width = width_for_bounds * 0.5;

    const BLRect trace_bounds(min_x - half_width, min_y - half_width,
                             max_x - min_x + width_for_bounds, max_y - min_y + width_for_bounds);

    // Performance optimization: Early culling check
    if (!AreRectsIntersecting(trace_bounds, world_view_rect)) {
        ++m_elements_culled_;
        return;  // Cull this trace
    }

    // Performance optimization: Determine final width once
    const double final_width = (thickness_override > 0.0) ? thickness_override :
                              (trace_width > 0 ? trace_width : kDefaultTraceWidth);

    // Performance optimization: Use optimized state management
    SetStrokeWidthOptimized(bl_ctx, final_width);
    bl_ctx.setStrokeStartCap(start_cap);
    bl_ctx.setStrokeEndCap(end_cap);
    bl_ctx.setStrokeJoin(BL_STROKE_JOIN_ROUND);
    bl_ctx.strokeLine(start_x, start_y, end_x, end_y);

    // Performance tracking
    ++m_elements_rendered_;
}

void RenderPipeline::RenderVia(BLContext& bl_ctx, const Via& via, const Board& board, const BLRect& world_view_rect, const BLRgba32& color_from, const BLRgba32& color_to)
{
    // Performance optimization: Cache via coordinates and radii
    const double via_x = via.GetX();
    const double via_y = via.GetY();
    const double radius_from = via.GetPadRadiusFrom();
    const double radius_to = via.GetPadRadiusTo();

    // Performance optimization: Calculate bounding box once
    const double max_radius = std::max(radius_from, radius_to);
    const double effective_radius = (max_radius > 0) ? max_radius : kMinViaExtent;
    const double diameter = 2.0 * effective_radius;

    const BLRect via_bounds(via_x - effective_radius, via_y - effective_radius, diameter, diameter);

    // Performance optimization: Early culling check
    if (!AreRectsIntersecting(via_bounds, world_view_rect)) {
        return;  // Cull this via
    }

    // Performance optimization: Batch layer visibility checks
    const Board::LayerInfo* layer_from_props = board.GetLayerById(via.GetLayerFrom());
    const Board::LayerInfo* layer_to_props = board.GetLayerById(via.GetLayerTo());

    const bool render_from_pad = layer_from_props && layer_from_props->IsVisible() && radius_from > 0;
    const bool render_to_pad = layer_to_props && layer_to_props->IsVisible() && radius_to > 0;

    // Performance optimization: Render pads with minimal state changes
    if (render_from_pad) {
        bl_ctx.setFillStyle(color_from);
        bl_ctx.fillCircle(via_x, via_y, radius_from);
    }

    if (render_to_pad) {
        bl_ctx.setFillStyle(color_to);
        bl_ctx.fillCircle(via_x, via_y, radius_to);
    }
    // Drill hole rendering can be added here if desired.
}

void RenderPipeline::RenderArc(BLContext& bl_ctx, const Arc& arc, const BLRect& world_view_rect, double thickness_override)
{
    // AABB for the arc (approximated by the bounding box of its circle)
    // More precise AABB would involve checking arc extents, but this is usually sufficient for culling.
    double radius = arc.GetRadius();
    double thickness_for_aabb = arc.GetThickness() > 0 ? arc.GetThickness() : kDefaultArcThickness;
    BLRect arc_aabb(arc.GetCenterX() - radius - thickness_for_aabb / 2.0, arc.GetCenterY() - radius - thickness_for_aabb / 2.0, 2 * radius + thickness_for_aabb, 2 * radius + thickness_for_aabb);

    if (!AreRectsIntersecting(arc_aabb, world_view_rect)) {
        return;  // Cull this arc
    }

    // Color is set by RenderBoard.
    double final_thickness;
    if (thickness_override > 0.0) {
        final_thickness = thickness_override;  // Use override thickness (e.g., for board outline)
    } else {
        final_thickness = arc.GetThickness();
        if (final_thickness <= 0) {
            final_thickness = kDefaultArcThickness;  // Default thickness.
        }
    }
    bl_ctx.setStrokeWidth(final_thickness);
    double start_angle_rad = arc.GetStartAngle() * (kPi / 180.0);
    double end_angle_rad = arc.GetEndAngle() * (kPi / 180.0);
    double sweep_angle_rad = end_angle_rad - start_angle_rad;

    // Ensure sweep_angle_rad is positive and represents the CCW sweep from start to end.
    // If end_angle_rad is numerically smaller than start_angle_rad (e.g. arc from 350deg to 10deg),
    // sweep_angle_rad would be negative. Adding 2*PI makes it positive for CCW direction.
    if (sweep_angle_rad < 0) {
        sweep_angle_rad += 2 * kPi;
    }
    // If the absolute value of sweep_angle_rad is very close to 2*PI or 0, it might indicate a full circle or no arc.
    // This logic assumes a proper arc segment is intended.

    BLPath path;
    path.arcTo(arc.GetCenterX(), arc.GetCenterY(), arc.GetRadius(), arc.GetRadius(), start_angle_rad, sweep_angle_rad);
    bl_ctx.strokePath(path);
}

void RenderPipeline::RenderComponent(BLContext& bl_ctx,
                                     const Component& component,
                                     const Board& board,
                                     const BLRect& world_view_rect,
                                     const BLRgba32& component_fill_color,
									 const BLRgba32& component_stroke_color,
                                     const std::unordered_map<BoardDataManager::ColorType, BLRgba32>& theme_color_cache,
                                     int selected_net_id,
                                     const Element* selected_element)
{
    // Performance optimization: Cache component properties to avoid repeated member access
    const double comp_w = (component.width > 0) ? component.width : kDefaultComponentMinDimension;
    const double comp_h = (component.height > 0) ? component.height : kDefaultComponentMinDimension;
    const double comp_cx = component.center_x;
    const double comp_cy = component.center_y;
    const double comp_rot_rad = component.rotation * (kPi / 180.0);

    // Performance optimization: Pre-compute trigonometric values
    const double cos_r = std::cos(comp_rot_rad);
    const double sin_r = std::sin(comp_rot_rad);

    // Performance optimization: Optimized bounding box calculation
    // Pre-compute half dimensions to avoid repeated division
    const double half_w = comp_w * 0.5;
    const double half_h = comp_h * 0.5;

    // Calculate rotated bounding box extents more efficiently
    const double abs_cos_r = std::abs(cos_r);
    const double abs_sin_r = std::abs(sin_r);
    const double rotated_half_w = half_w * abs_cos_r + half_h * abs_sin_r;
    const double rotated_half_h = half_w * abs_sin_r + half_h * abs_cos_r;

    // Create axis-aligned bounding box
    const BLRect component_world_bounds(comp_cx - rotated_half_w, comp_cy - rotated_half_h,
                                       2.0 * rotated_half_w, 2.0 * rotated_half_h);

    // Performance optimization: Early culling check
    if (!AreRectsIntersecting(component_world_bounds, world_view_rect)) {
        return;  // Cull entire component
    }

    // Draw component outline using Blend2D path
    BLPath outline;
    outline.moveTo(component.center_x - comp_w / 2.0, component.center_y - comp_h / 2.0);
    outline.lineTo(component.center_x + comp_w / 2.0, component.center_y - comp_h / 2.0);
    outline.lineTo(component.center_x + comp_w / 2.0, component.center_y + comp_h / 2.0);
    outline.lineTo(component.center_x - comp_w / 2.0, component.center_y + comp_h / 2.0);
    outline.close();

    // Determine actual highlight color from theme cache for comparison
    BLRgba32 actual_highlight_color = theme_color_cache.count(BoardDataManager::ColorType::kNetHighlight) ? theme_color_cache.at(BoardDataManager::ColorType::kNetHighlight)
                                                                                                          : BLRgba32(0xFFFFFF00);  // Fallback must be consistent with RenderBoard

    BLRgba32 fill_color;
    if (component_fill_color.value == actual_highlight_color.value) {  // Exact match for highlight color
        // Component is selected/highlighted: use translucent highlight color for fill
        fill_color = BLRgba32(component_fill_color.r(), component_fill_color.g(), component_fill_color.b(), 3*(component_fill_color.a()/4));  // 50% alpha
    } else {
        // Component is not selected: use theme color with a standard low alpha for fill
        fill_color = BLRgba32(component_fill_color.r(), component_fill_color.g(), component_fill_color.b(), component_fill_color.a());  // ~12.5% alpha, consistent with old examples
    }

    bl_ctx.setFillStyle(fill_color);
    bl_ctx.fillPath(outline);

    // Stroke is always the component_stroke_color (which is already correctly highlight or theme)
    bl_ctx.setStrokeStyle(component_stroke_color);

    // Set stroke thickness from BoardDataManager
    float component_stroke_thickness = 0.05f;  // Default thickness
    if (m_render_context_ && m_render_context_->GetBoardDataManager()) {
        auto board_data_manager = m_render_context_->GetBoardDataManager();
        component_stroke_thickness = board_data_manager->GetComponentStrokeThickness();
    }
    bl_ctx.setStrokeWidth(component_stroke_thickness);
    bl_ctx.strokePath(outline);

    // Performance optimization: Pre-compute pin colors to avoid repeated map lookups
    const BLRgba32 pin_highlight_for_pins = theme_color_cache.count(BoardDataManager::ColorType::kNetHighlight) ?
        theme_color_cache.at(BoardDataManager::ColorType::kNetHighlight) : BLRgba32(0xFFFFFFFF);
    const BLRgba32 selected_element_highlight_for_pins = theme_color_cache.count(BoardDataManager::ColorType::kSelectedElementHighlight) ?
        theme_color_cache.at(BoardDataManager::ColorType::kSelectedElementHighlight) : BLRgba32(0xFFFFFF00);
    const BLRgba32 default_pin_fill_color = theme_color_cache.count(BoardDataManager::ColorType::kPinFill) ?
        theme_color_cache.at(BoardDataManager::ColorType::kPinFill) : BLRgba32(0xC0999999);
    const BLRgba32 default_pin_stroke_color = theme_color_cache.count(BoardDataManager::ColorType::kPinStroke) ?
        theme_color_cache.at(BoardDataManager::ColorType::kPinStroke) : BLRgba32(0xC0000000);

    // Performance optimization: Pre-compute special net colors
    BLRgba32 gnd_color = default_pin_fill_color;
    BLRgba32 nc_color = default_pin_fill_color;
    if (m_render_context_ && m_render_context_->GetBoardDataManager()) {
        auto board_data_manager = m_render_context_->GetBoardDataManager();
        gnd_color = board_data_manager->GetColor(BoardDataManager::ColorType::kGND);
        nc_color = board_data_manager->GetColor(BoardDataManager::ColorType::kNC);
    }

    // Performance optimization: Batch pin rendering with reduced overhead
    for (const auto& pin_ptr : component.pins) {
        if (!pin_ptr || !pin_ptr->IsVisible()) {
            continue;
        }

        // Performance optimization: Check layer visibility once per pin
        const Board::LayerInfo* pin_layer = board.GetLayerById(pin_ptr->GetLayerId());
        if (!pin_layer || !pin_layer->IsVisible()) {
            continue;
        }

        // Performance optimization: Batch selection state checks
        const bool is_pin_selected_element = (selected_element == pin_ptr.get());
        const bool is_pin_selected_net = (selected_net_id != -1 && pin_ptr->GetNetId() == selected_net_id);

        // Performance optimization: Get net name once and use for color determination
        const std::string& net_name = pin_ptr->GetNetName(board);

        BLRgba32 final_fill_color, final_stroke_color;

        if (is_pin_selected_element) {
            // Selected element takes priority over net highlighting
            final_fill_color = final_stroke_color = selected_element_highlight_for_pins;
        } else if (is_pin_selected_net) {
            final_fill_color = final_stroke_color = pin_highlight_for_pins;
        } else if (net_name == "GND") {
            final_fill_color = final_stroke_color = gnd_color;
        } else if (net_name == "NC") {
            final_fill_color = final_stroke_color = nc_color;
        } else {
            final_fill_color = default_pin_fill_color;
            final_stroke_color = default_pin_stroke_color;
        }
		

        // Performance optimization: Batch Blend2D state changes for pin rendering
        bl_ctx.setFillStyle(final_fill_color);
        bl_ctx.setStrokeStyle(final_stroke_color);

        // Set stroke thickness from BoardDataManager (cached from render context)
        float stroke_thickness = 0.03f;  // Default thickness
        if (m_render_context_ && m_render_context_->GetBoardDataManager()) {
            auto board_data_manager = m_render_context_->GetBoardDataManager();
            stroke_thickness = board_data_manager->GetPinStrokeThickness();
        }
        bl_ctx.setStrokeWidth(stroke_thickness);

        RenderPin(bl_ctx, *pin_ptr, &component, final_fill_color, final_stroke_color, board);
    }

    // Component text label rendering disabled - using ImGui tooltips instead
    // for (const auto& label_ptr : component.text_labels) {
    //     // Text labels are now handled via ImGui tooltips
    // }
}

// Enhanced font caching system
BLFont& RenderPipeline::GetCachedFont(const std::string& font_family, float size) {
    std::lock_guard<std::mutex> lock(m_font_cache_mutex_);

    FontCacheKey key{font_family, size};
    auto it = m_font_cache_.find(key);
    if (it != m_font_cache_.end()) {
        return it->second;
    }

    // Font not in cache, create it
    BLFontFace face;
    BLResult err = BL_SUCCESS;

    // Try to find cached font face
    auto face_it = m_font_face_cache_.find(font_family);
    if (face_it != m_font_face_cache_.end()) {
        face = face_it->second;
    } else if (!font_family.empty()) {
        err = face.createFromFile(font_family.c_str());
        if (err == BL_SUCCESS) {
            m_font_face_cache_[font_family] = face;
        }
    }

    // If specific font failed or was not provided, try fallback fonts
    if (err != BL_SUCCESS || font_family.empty() || !face.isValid()) {
        // Try system fonts and common paths
        const std::vector<std::string> fallbackFonts = {
            "DejaVuSans.ttf",
            "arial.ttf",
            "LiberationSans-Regular.ttf",
            "C:/Windows/Fonts/arial.ttf",           // Windows
            "C:/Windows/Fonts/calibri.ttf",         // Windows
            "/System/Library/Fonts/Arial.ttf",      // macOS
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",  // Linux
            "/usr/share/fonts/TTF/DejaVuSans.ttf"    // Linux alternative
        };

        bool loadedFallback = false;
        for (const std::string& fontName : fallbackFonts) {
            auto fallback_it = m_font_face_cache_.find(fontName);
            if (fallback_it != m_font_face_cache_.end()) {
                face = fallback_it->second;
                if (face.isValid()) {
                    loadedFallback = true;
                    break;
                }
            }
            err = face.createFromFile(fontName.c_str());
            if (err == BL_SUCCESS) {
                m_font_face_cache_[fontName] = face;
                loadedFallback = true;
                std::cout << "RenderPipeline: Successfully loaded fallback font: " << fontName << std::endl;
                break;
            }
        }
        if (!loadedFallback) {
            // Create a dummy font to avoid repeated failures
            static BLFont dummy_font;
            static bool warned = false;
            if (!warned) {
                std::cerr << "RenderPipeline: Warning - No fonts could be loaded, text rendering will be disabled" << std::endl;
                warned = true;
            }
            return dummy_font;
        }
    }

    // Create font from face and cache it
    BLFont font;
    font.createFromFace(face, size);
    auto [inserted_it, success] = m_font_cache_.emplace(key, std::move(font));
    return inserted_it->second;
}

void RenderPipeline::PreloadCommonFonts() {
    // Preload common font sizes to avoid runtime font creation
    const std::vector<std::string> common_fonts = {
        "DejaVuSans.ttf", "arial.ttf", "LiberationSans-Regular.ttf"
    };
    const std::vector<float> common_sizes = {8.0f, 10.0f, 12.0f, 14.0f, 16.0f, 18.0f, 24.0f};

    for (const auto& font_family : common_fonts) {
        for (float size : common_sizes) {
            GetCachedFont(font_family, size);
        }
    }
}

void RenderPipeline::RenderTextLabel(BLContext& bl_ctx, const TextLabel& text_label, const BLRgba32& color)
{
    if (!text_label.IsVisible() || text_label.text_content.empty()) {
        return;
    }

    // Use cached font instead of creating new one every frame
    float final_size = static_cast<float>(text_label.font_size * text_label.scale);
    BLFont& font = GetCachedFont(text_label.font_family, final_size);

    bl_ctx.setFillStyle(color);

    if (text_label.rotation != 0.0) {
        bl_ctx.save();
        bl_ctx.translate(text_label.coords.x_ax, text_label.coords.y_ax);
        bl_ctx.rotate(text_label.rotation * (kPi / 180.0));
        bl_ctx.fillUtf8Text(BLPoint(0, 0), font, text_label.text_content.c_str());
        bl_ctx.restore();
    } else {
        bl_ctx.fillUtf8Text(BLPoint(text_label.coords.x_ax, text_label.coords.y_ax + text_label.font_size), font, text_label.text_content.c_str());
    }
}


// Render a pin using Blend2D, automatically handling all shape types
void RenderPipeline::RenderPin(BLContext& ctx, const Pin& pin, const Component* parent_component, const BLRgba32& fill_color, const BLRgba32& stroke_color, const Board& board)
{
    // Get pin dimensions (these are in the pin's local coordinate system)
    auto [pin_width, pin_height] = pin.GetDimensions();  // Should return dimensions pre-rotation
    double x_coord = pin.coords.x_ax;
    double y_coord = pin.coords.y_ax;
    double rotation = pin.rotation;



    // Alternative approach: Consider pin's natural orientation
    // Some PCB formats might store rotation relative to the pin's natural orientation
    // For rectangular pins, natural orientation might be width > height = horizontal
    bool pin_is_naturally_horizontal = (pin_width > pin_height);
    double effective_rotation = rotation;

    // Test: Adjust rotation based on pin's natural orientation
    if (pin_is_naturally_horizontal && std::abs(rotation - 90.0) < 0.1) {
        // If pin is naturally horizontal and rotation is 90°, it should become vertical
        effective_rotation = rotation;  // Keep as-is for now
    } else if (!pin_is_naturally_horizontal && std::abs(rotation - 90.0) < 0.1) {
        // If pin is naturally vertical and rotation is 90°, it should become horizontal
        effective_rotation = rotation;  // Keep as-is for now
    }


    ctx.setFillStyle(fill_color);
	ctx.setStrokeStyle(stroke_color);

    // Performance optimization: Only apply rotation if significant and for non-circular shapes
    bool needs_rotation = (std::abs(effective_rotation) > 0.01) && !std::holds_alternative<CirclePad>(pin.pad_shape);

    if (needs_rotation) {
        ctx.save();  // Save current transformation state



        // Translate to pin center, rotate, then translate back
        ctx.translate(x_coord, y_coord);
        // Test: Try negative rotation to see if direction is the issue
        ctx.rotate(-effective_rotation * (kPi / 180.0));  // Convert degrees to radians (negative for testing)
        ctx.translate(-x_coord, -y_coord);
    }


    // Render based on shape type
    std::visit(
        [&ctx, pin_width, pin_height, x_coord, y_coord, fill_color, stroke_color](const auto& shape) {
            using T = std::decay_t<decltype(shape)>;

            if constexpr (std::is_same_v<T, CirclePad>) {
                // Circles don't need rotation - always render the same regardless of rotation
                BLCircle circle(x_coord, y_coord, shape.radius);
                ctx.fillCircle(circle);
				ctx.strokeCircle(circle);
            } else if constexpr (std::is_same_v<T, RectanglePad>) {
                BLRect rect(x_coord - pin_width / 2.0, y_coord - pin_height / 2.0, pin_width, pin_height);
                ctx.fillRect(rect);
				ctx.strokeRect(rect);
            } else if constexpr (std::is_same_v<T, CapsulePad>) {
                // Call renderCapsule, which expects to draw centered at (0,0)
                // with the pin's local width and height.
                RenderCapsule(ctx, pin_width, pin_height, x_coord, y_coord, fill_color, stroke_color);
            }
		}
        ,
        pin.pad_shape);

    if (needs_rotation) {
        ctx.restore();  // Restore transformation state
    }
}




static void RenderCapsule(BLContext& ctx, double width, double height, double x_coord, double y_coord, const BLRgba32& fill_color, const BLRgba32& stroke_color)
{
    double radius = std::min(width, height) / 2.0;  // Radius is half the smaller dimension

    // Create a rounded rectangle centered at x_coord, y_coord
    BLRoundRect capsule(x_coord - (width / 2.0), y_coord - (height / 2.0), width, height, radius);
	ctx.setFillStyle(fill_color);
	ctx.setStrokeStyle(stroke_color);
    ctx.fillRoundRect(capsule);
	ctx.strokeRoundRect(capsule);
}

// ThreadPool implementation
ThreadPool::ThreadPool(size_t num_threads) : stop_(false), active_tasks_(0) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

                    if (stop_ && tasks_.empty()) {
                        return;
                    }

                    task = std::move(tasks_.front());
                    tasks_.pop();
                    ++active_tasks_;
                }

                task();

                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    --active_tasks_;
                    completion_cv_.notify_all();
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();

    for (std::thread& worker : workers_) {
        worker.join();
    }
}



void ThreadPool::wait_for_all() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    completion_cv_.wait(lock, [this] { return tasks_.empty() && active_tasks_ == 0; });
}

// Enhanced multi-threading implementation
void RenderPipeline::InitializeThreadPool()
{
    if (m_thread_pool_) {
        return; // Already initialized
    }

    if (m_threading_enabled_ && m_thread_count_ > 0) {
        m_thread_pool_ = std::make_unique<ThreadPool>(m_thread_count_);
        std::cout << "RenderPipeline: Thread pool initialized with " << m_thread_count_ << " threads" << std::endl;

        // Preload common fonts in background
        PreloadCommonFonts();
    }
}

void RenderPipeline::ShutdownThreadPool()
{
    if (m_thread_pool_) {
        m_thread_pool_.reset();
        std::cout << "RenderPipeline: Thread pool shutdown" << std::endl;
    }
    m_threading_enabled_ = false;
}



void RenderPipeline::RenderTracesWithHighlighting(const std::vector<const Trace*>& traces,
                                                  const BLRgba32& base_color,
                                                  const BLRect& world_view_rect,
                                                  BLStrokeCap start_cap,
                                                  BLStrokeCap end_cap,
                                                  double thickness_override,
                                                  int selected_net_id,
                                                  const Element* selected_element,
                                                  const BLRgba32& highlight_color,
                                                  const BLRgba32& selected_element_highlight_color)
{
    if (traces.empty()) return;

    auto& ctx = m_render_context_->GetBlend2DContext();

    // Group traces by color and thickness to minimize state changes while preserving highlighting
    // Use a simpler approach: separate containers for different colors
    std::vector<const Trace*> normal_traces;
    std::vector<const Trace*> highlighted_traces;
    std::vector<const Trace*> selected_traces;

    for (const Trace* trace : traces) {
        if (!trace) continue;

        // Determine individual trace highlighting state
        bool is_selected_element = (selected_element == trace);
        bool is_selected_net = (selected_net_id != -1 && trace->GetNetId() == selected_net_id);

        if (is_selected_element) {
            // Selected element takes priority over net highlighting
            selected_traces.push_back(trace);
        } else if (is_selected_net) {
            highlighted_traces.push_back(trace);
        } else {
            normal_traces.push_back(trace);
        }
    }

    // Helper lambda to render a group of traces with the same color
    auto render_trace_group = [&](const std::vector<const Trace*>& group_traces, const BLRgba32& group_color) {
        if (group_traces.empty()) return;

        // Group by thickness to minimize state changes
        std::map<double, std::vector<const Trace*>> thickness_groups;
        for (const Trace* trace : group_traces) {
            if (!trace) continue;
            double final_thickness = (thickness_override > 0.0) ? thickness_override :
                                    (trace->GetWidth() > 0 ? trace->GetWidth() : kDefaultTraceWidth);
            thickness_groups[final_thickness].push_back(trace);
        }

        // Render each thickness group
        for (const auto& [thickness, traces_in_group] : thickness_groups) {
            if (traces_in_group.empty()) continue;

            // Set rendering state once for this thickness group
            ctx.setStrokeStyle(group_color);
            ctx.setStrokeWidth(thickness);
            ctx.setStrokeStartCap(start_cap);
            ctx.setStrokeEndCap(end_cap);
            ctx.setStrokeJoin(BL_STROKE_JOIN_ROUND);

            // Use batched path rendering for performance
            BLPath batch_path;
            int visible_count = 0;

            for (const Trace* trace : traces_in_group) {
                if (!trace) continue;

                // Viewport culling
                const double start_x = trace->GetStartX();
                const double start_y = trace->GetStartY();
                const double end_x = trace->GetEndX();
                const double end_y = trace->GetEndY();

                const double min_x = std::min(start_x, end_x);
                const double max_x = std::max(start_x, end_x);
                const double min_y = std::min(start_y, end_y);
                const double max_y = std::max(start_y, end_y);

                const double half_width = thickness * 0.5;
                const BLRect trace_bounds(min_x - half_width, min_y - half_width,
                                         max_x - min_x + thickness, max_y - min_y + thickness);

                if (!AreRectsIntersecting(trace_bounds, world_view_rect)) {
                    continue; // Cull this trace
                }

                // Add line to batch path
                batch_path.moveTo(start_x, start_y);
                batch_path.lineTo(end_x, end_y);
                visible_count++;
                m_elements_rendered_++;
            }

            // Render entire batch with single stroke call
            if (visible_count > 0) {
                ctx.strokePath(batch_path);
            }
        }
    };

    // Render traces in order: normal, highlighted, selected (so selected appears on top)
    render_trace_group(normal_traces, base_color);
    render_trace_group(highlighted_traces, highlight_color);
    render_trace_group(selected_traces, selected_element_highlight_color);
}



void RenderPipeline::RenderComponentsOptimized(const std::vector<const Component*>& components,
                                              const Board& board,
                                              const BLRect& world_view_rect,
                                              const std::unordered_map<BoardDataManager::ColorType, BLRgba32>& theme_colors,
                                              int selected_net_id,
                                              const Element* selected_element)
{
    if (components.empty()) return;

    auto& ctx = m_render_context_->GetBlend2DContext();

    // Performance optimization: Group components by selection state to minimize state changes
    std::vector<const Component*> normal_components;
    std::vector<const Component*> selected_net_components;
    std::vector<const Component*> selected_element_components;

    // Pre-allocate vectors for performance
    normal_components.reserve(components.size());
    selected_net_components.reserve(components.size() / 10); // Estimate
    selected_element_components.reserve(10); // Usually very few

    // Performance optimization: Categorize components by selection state with viewport culling
    size_t culled_count = 0;
    for (const Component* component : components) {
        if (!component) continue;

        // Early viewport culling with simple AABB check
        const double comp_w = (component->width > 0) ? component->width : kDefaultComponentMinDimension;
        const double comp_h = (component->height > 0) ? component->height : kDefaultComponentMinDimension;
        const double comp_cx = component->center_x;
        const double comp_cy = component->center_y;

        // Simple AABB check for culling (could be enhanced with rotation later)
        const double half_w = comp_w * 0.5;
        const double half_h = comp_h * 0.5;
        const BLRect component_bounds(comp_cx - half_w, comp_cy - half_h, comp_w, comp_h);

        if (!AreRectsIntersecting(component_bounds, world_view_rect)) {
            culled_count++;
            m_elements_culled_++;
            continue; // Cull this component
        }

        // Categorize by selection state
        bool is_selected_element = (selected_element == component);
        bool is_selected_net = false;

        if (selected_net_id != -1) {
            for (const auto& pin_ptr : component->pins) {
                if (pin_ptr && pin_ptr->GetNetId() == selected_net_id) {
                    is_selected_net = true;
                    break;
                }
            }
        }

        if (is_selected_element) {
            selected_element_components.push_back(component);
        } else if (is_selected_net) {
            selected_net_components.push_back(component);
        } else {
            normal_components.push_back(component);
        }
    }

    // Performance optimization: Batch render each group with optimized state management
    auto render_component_batch = [&](const std::vector<const Component*>& batch, const BLRgba32& fill_color, const BLRgba32& stroke_color) {
        if (batch.empty()) return;

        for (const Component* component : batch) {
            RenderComponent(ctx, *component, board, world_view_rect, fill_color, stroke_color, theme_colors, selected_net_id, selected_element);
            m_elements_rendered_++;
        }
    };

    // Render normal components
    auto fill_it = theme_colors.find(BoardDataManager::ColorType::kComponentFill);
    auto stroke_it = theme_colors.find(BoardDataManager::ColorType::kComponentStroke);
    BLRgba32 normal_fill = (fill_it != theme_colors.end()) ? fill_it->second : BLRgba32(0xFF007BFF);
    BLRgba32 normal_stroke = (stroke_it != theme_colors.end()) ? stroke_it->second : BLRgba32(0xFF000000);
    render_component_batch(normal_components, normal_fill, normal_stroke);

    // Render selected net components
    auto net_it = theme_colors.find(BoardDataManager::ColorType::kNetHighlight);
    BLRgba32 net_color = (net_it != theme_colors.end()) ? net_it->second : BLRgba32(0xFFFFFF00);
    render_component_batch(selected_net_components, net_color, net_color);

    // Render selected element components
    auto element_it = theme_colors.find(BoardDataManager::ColorType::kSelectedElementHighlight);
    BLRgba32 element_color = (element_it != theme_colors.end()) ? element_it->second : BLRgba32(0xFFFFFF00);
    render_component_batch(selected_element_components, element_color, element_color);


}

// ============================================================================
// NEW OPTIMIZATION IMPLEMENTATIONS
// ============================================================================

void RenderPipeline::RenderWithLOD(BLContext& bl_ctx, const Board& board, const Camera& camera,
                                   const Viewport& viewport, const BLRect& world_view_rect)
{
    // Determine appropriate LOD level
    lod::LODLevel current_lod = m_lod_manager_.DetermineLOD(camera, viewport, board);
    m_lod_manager_.SetCurrentLOD(current_lod);

    // Apply LOD settings to context
    m_lod_manager_.ApplyLODToContext(bl_ctx, current_lod);

    // Reset performance counters
    m_lod_manager_.ResetCounters();

    // Render based on LOD level
    switch (current_lod) {
        case lod::LODLevel::kVeryLow:
            RenderBoardOutlineOnly(bl_ctx, board, world_view_rect);
            break;
        case lod::LODLevel::kLow:
            RenderBoardLowDetail(bl_ctx, board, camera, viewport, world_view_rect);
            break;
        case lod::LODLevel::kMedium:
            RenderBoardMediumDetail(bl_ctx, board, camera, viewport, world_view_rect);
            break;
        case lod::LODLevel::kHigh:
        case lod::LODLevel::kVeryHigh:
        default:
            // Use existing full detail rendering
            RenderBoard(bl_ctx, board, camera, viewport, world_view_rect);
            break;
    }
}

void RenderPipeline::RenderBoardOutlineOnly(BLContext& bl_ctx, const Board& board, const BLRect& world_view_rect)
{
    // Very low detail - only render board outline
    bl_ctx.save();

    // Get board outline color
    const RenderingState& render_state = GetCachedRenderingState(board);
    const auto& theme_color_cache = render_state.theme_color_cache;
    const BLRgba32 board_edges_color = theme_color_cache.count(BoardDataManager::ColorType::kBoardEdges) ?
        theme_color_cache.at(BoardDataManager::ColorType::kBoardEdges) : BLRgba32(0xFF00FF00);

    bl_ctx.setStrokeStyle(board_edges_color);
    bl_ctx.setStrokeWidth(render_state.board_outline_thickness);

    // Render only board outline elements (layer 28)
    auto layer_it = board.m_elements_by_layer.find(kBoardOutlineLayerId);
    if (layer_it != board.m_elements_by_layer.end()) {
        for (const auto& element_ptr : layer_it->second) {
            if (!element_ptr || !element_ptr->IsVisible()) continue;

            if (element_ptr->GetElementType() == ElementType::kArc) {
                if (auto arc = dynamic_cast<const Arc*>(element_ptr.get())) {
                    RenderArc(bl_ctx, *arc, world_view_rect, render_state.board_outline_thickness);
                    m_lod_manager_.IncrementRendered();
                }
            }
        }
    }

    bl_ctx.restore();
}

void RenderPipeline::RenderBoardLowDetail(BLContext& bl_ctx, const Board& board, const Camera& camera,
                                         const Viewport& viewport, const BLRect& world_view_rect)
{
    // Low detail - render basic shapes without fine details
    bl_ctx.save();
    bl_ctx.applyTransform(ViewMatrix(bl_ctx, camera, viewport));

    const RenderingState& render_state = GetCachedRenderingState(board);

    // Render only major elements with simplified representation
    // 1. Board outline
    RenderBoardOutlineOnly(bl_ctx, board, world_view_rect);

    // 2. Major traces only (width > threshold)
    const double min_trace_width = 0.2; // Only render traces thicker than 0.2mm
    std::vector<const Trace*> major_traces;

    for (int layer_id = Board::kTraceLayersStart; layer_id <= Board::kTraceLayersEnd; ++layer_id) {
        auto layer_it = board.m_elements_by_layer.find(layer_id);
        if (layer_it == board.m_elements_by_layer.end()) continue;

        const Board::LayerInfo* layer_info = board.GetLayerById(layer_id);
        if (!layer_info || !layer_info->IsVisible()) continue;

        for (const auto& element_ptr : layer_it->second) {
            if (!element_ptr || !element_ptr->IsVisible()) continue;

            if (element_ptr->GetElementType() == ElementType::kTrace) {
                if (auto trace = dynamic_cast<const Trace*>(element_ptr.get())) {
                    if (trace->GetWidth() >= min_trace_width) {
                        major_traces.push_back(trace);
                    } else {
                        m_lod_manager_.IncrementCulled();
                    }
                }
            }
        }
    }

    // Render major traces
    if (!major_traces.empty()) {
        const auto& layer_id_color_cache = render_state.layer_id_color_cache;
        const BLRgba32 base_color = layer_id_color_cache.count(1) ?
            layer_id_color_cache.at(1) : BLRgba32(0xFFFF0000);

        RenderTracesOptimized(major_traces, base_color, world_view_rect,
                             BL_STROKE_CAP_ROUND, BL_STROKE_CAP_ROUND, -1.0);
    }

    // 3. Components as simple rectangles
    RenderComponentsSimplified(bl_ctx, board, world_view_rect, render_state);

    bl_ctx.restore();
}

void RenderPipeline::RenderBoardMediumDetail(BLContext& bl_ctx, const Board& board, const Camera& camera,
                                            const Viewport& viewport, const BLRect& world_view_rect)
{
    // Medium detail - standard rendering with some optimizations
    // Use existing RenderBoard but with medium LOD settings applied
    RenderBoard(bl_ctx, board, camera, viewport, world_view_rect);
}

void RenderPipeline::RenderComponentsSimplified(BLContext& bl_ctx, const Board& board,
                                               const BLRect& world_view_rect, const RenderingState& render_state)
{
    // Render components as simple rectangles for low LOD
    const BLRgba32 component_color = render_state.theme_color_cache.count(BoardDataManager::ColorType::kComponentFill) ?
        render_state.theme_color_cache.at(BoardDataManager::ColorType::kComponentFill) : BLRgba32(0xFF007BFF);

    bl_ctx.setFillStyle(component_color);

    // Render components from both component layers
    std::vector<int> component_layers = {Board::kTopCompLayer, Board::kBottomCompLayer};

    for (int layer_id : component_layers) {
        auto layer_it = board.m_elements_by_layer.find(layer_id);
        if (layer_it == board.m_elements_by_layer.end()) continue;

        const Board::LayerInfo* layer_info = board.GetLayerById(layer_id);
        if (!layer_info || !layer_info->IsVisible()) continue;

        for (const auto& element_ptr : layer_it->second) {
            if (!element_ptr || !element_ptr->IsVisible()) continue;

            if (element_ptr->GetElementType() == ElementType::kComponent) {
                if (auto component = dynamic_cast<const Component*>(element_ptr.get())) {
                    // Get component bounding box
                    BLRect bbox = component->GetBoundingBox();

                    // Cull if not visible
                    if (!AreRectsIntersecting(bbox, world_view_rect)) {
                        m_lod_manager_.IncrementCulled();
                        continue;
                    }

                    // Render as simple rectangle
                    bl_ctx.fillRect(bbox);
                    m_lod_manager_.IncrementRendered();
                }
            }
        }
    }
}

void RenderPipeline::RenderWithCaching(BLContext& bl_ctx, const Board& board, const Camera& camera,
                                      const Viewport& viewport, const BLRect& world_view_rect)
{
    // Update dirty region tracking
    UpdateDirtyRegions(camera, viewport, board);

    // Check if we can use cached rendering
    if (ShouldUseCache(camera, viewport, board) && !m_dirty_tracker_.NeedsRedraw()) {
        // Use cached image - massive performance improvement!
        if (!m_cached_board_render_.cached_image.empty()) {
            bl_ctx.blitImage(BLPoint(0, 0), m_cached_board_render_.cached_image);
            return;
        }
    }

    // Need to render fresh - create cache
    BLImage cache_image(static_cast<int>(viewport.GetWidth()), static_cast<int>(viewport.GetHeight()), BL_FORMAT_PRGB32);
    BLContext cache_ctx(cache_image);

    // Render to cache
    if (m_lod_manager_.IsInteractiveMode()) {
        RenderWithLOD(cache_ctx, board, camera, viewport, world_view_rect);
    } else {
        RenderBoard(cache_ctx, board, camera, viewport, world_view_rect);
    }

    // Update cache
    m_cached_board_render_.cached_image = cache_image;
    m_cached_board_render_.cached_viewport = BLRect(0, 0, viewport.GetWidth(), viewport.GetHeight());
    m_cached_board_render_.cached_zoom = camera.GetZoom();
    m_cached_board_render_.cached_pan = BLPoint(camera.GetPosition().x_ax, camera.GetPosition().y_ax);
    m_cached_board_render_.is_valid = true;

    // Clear dirty flags
    m_dirty_tracker_.ClearFlags();

    // Blit cached image to main context
    bl_ctx.blitImage(BLPoint(0, 0), cache_image);
}

void RenderPipeline::UpdateDirtyRegions(const Camera& camera, const Viewport& viewport, const Board& board)
{
    // Check for changes that require redraw
    BLRect current_viewport(0, 0, viewport.GetWidth(), viewport.GetHeight());
    double current_zoom = camera.GetZoom();
    BLPoint current_pan(camera.GetPosition().x_ax, camera.GetPosition().y_ax);

    // Check zoom changes
    if (std::abs(current_zoom - m_dirty_tracker_.last_zoom_level) > 0.001) {
        m_dirty_tracker_.zoom_changed = true;
        m_dirty_tracker_.last_zoom_level = current_zoom;
    }

    // Check pan changes
    if (std::abs(current_pan.x - m_dirty_tracker_.last_pan_position.x) > 0.1 ||
        std::abs(current_pan.y - m_dirty_tracker_.last_pan_position.y) > 0.1) {
        m_dirty_tracker_.pan_changed = true;
        m_dirty_tracker_.last_pan_position = current_pan;
    }

    // Check viewport changes
    if (std::abs(current_viewport.w - m_dirty_tracker_.last_viewport_rect.w) > 0.1 ||
        std::abs(current_viewport.h - m_dirty_tracker_.last_viewport_rect.h) > 0.1) {
        m_dirty_tracker_.full_redraw_needed = true;
        m_dirty_tracker_.last_viewport_rect = current_viewport;
    }
}

bool RenderPipeline::ShouldUseCache(const Camera& camera, const Viewport& viewport, const Board& board) const
{
    // Check if we have a valid cached render that matches current state
    BLRect current_viewport(0, 0, viewport.GetWidth(), viewport.GetHeight());
    BLPoint current_pan(camera.GetPosition().x_ax, camera.GetPosition().y_ax);
    double current_zoom = camera.GetZoom();

    // Get current board state
    auto bdm = m_render_context_->GetBoardDataManager();
    if (!bdm) return false;

    std::shared_ptr<const Board> current_board = bdm->GetBoard();
    int current_selected_net = bdm->GetSelectedNetId();
    const Element* current_selected_element = bdm->GetSelectedElement();

    return m_cached_board_render_.IsValidFor(current_viewport, current_zoom, current_pan,
                                           current_board, current_selected_net, current_selected_element,
                                           std::vector<bool>{}); // TODO: Add layer visibility tracking
}

void RenderPipeline::RebuildSpatialIndex(const Board& board)
{
    if (!m_spatial_index_dirty_) return;

    // Collect all visible elements
    std::vector<const Element*> all_elements;

    for (const auto& layer_pair : board.m_elements_by_layer) {
        const Board::LayerInfo* layer_info = board.GetLayerById(layer_pair.first);
        if (!layer_info || !layer_info->IsVisible()) continue;

        for (const auto& element_ptr : layer_pair.second) {
            if (element_ptr && element_ptr->IsVisible()) {
                all_elements.push_back(element_ptr.get());
            }
        }
    }

    // Rebuild spatial index
    m_hit_detector_.RebuildIndex(all_elements);
    m_spatial_index_dirty_ = false;
}

const Element* RenderPipeline::FindHitElementOptimized(const Vec2& world_pos, float tolerance, const Component* parent_component)
{
    // Ensure spatial index is up to date
    if (m_spatial_index_dirty_ && m_render_context_ && m_render_context_->GetBoardDataManager()) {
        auto bdm = m_render_context_->GetBoardDataManager();
        auto board = bdm->GetBoard();
        if (board) {
            RebuildSpatialIndex(*board);
        }
    }

    // Use optimized hit detection
    return m_hit_detector_.FindHitElement(world_pos, tolerance, parent_component);
}

void RenderPipeline::RenderTracesOptimized(const std::vector<const Trace*>& traces,
                                          const BLRgba32& color,
                                          const BLRect& world_view_rect,
                                          BLStrokeCap start_cap,
                                          BLStrokeCap end_cap,
                                          double thickness_override)
{
    if (traces.empty()) return;

    auto& ctx = m_render_context_->GetBlend2DContext();

    // IMPORTANT: Based on Blend2D multithreading documentation, we should let Blend2D
    // handle its own multithreading rather than creating our own thread-local contexts.
    // Blend2D's asynchronous rendering is more efficient than manual threading.

    // Use Blend2D's asynchronous rendering with batched operations
    // Note: Blend2D handles threading internally, so we can use the async version for all cases
    RenderTracesBatchedAsync(ctx, traces, color, world_view_rect, start_cap, end_cap, thickness_override);
}

void RenderPipeline::RenderSingleTraceOptimized(BLContext& bl_ctx, const Trace* trace, double thickness_override)
{
    if (!trace) return;

    // Create cache key for this trace
    std::string trace_id = std::to_string(reinterpret_cast<uintptr_t>(trace));
    double final_thickness = (thickness_override > 0.0) ? thickness_override :
                            (trace->GetWidth() > 0 ? trace->GetWidth() : kDefaultTraceWidth);

    auto cache_key = path_cache::BLPathCache::CreateTraceKey(trace_id, final_thickness,
                                                            BL_STROKE_CAP_ROUND, BL_STROKE_CAP_ROUND);

    // Create path for this trace
    BLPath trace_path;
    trace_path.moveTo(trace->GetStartX(), trace->GetStartY());
    trace_path.lineTo(trace->GetEndX(), trace->GetEndY());

    // Set up stroke options
    BLStrokeOptions stroke_opts;
    stroke_opts.width = final_thickness;
    stroke_opts.startCap = BL_STROKE_CAP_ROUND;
    stroke_opts.endCap = BL_STROKE_CAP_ROUND;

    // Get cached stroked path
    const BLPath& stroked_path = path_cache::g_path_cache.GetStrokedPath(cache_key, trace_path, stroke_opts);

    // Render the cached path
    bl_ctx.fillPath(stroked_path);
}

void RenderPipeline::DivideTracesIntoSpatialBuckets(const std::vector<const Trace*>& traces,
                                                   const BLRect& world_view_rect,
                                                   std::vector<std::vector<const Trace*>>& buckets) const
{
    // Create spatial buckets based on thread count
    size_t num_buckets = std::min(static_cast<size_t>(m_thread_count_), traces.size());
    buckets.resize(num_buckets);

    // Reserve space in each bucket
    size_t traces_per_bucket = traces.size() / num_buckets + 1;
    for (auto& bucket : buckets) {
        bucket.reserve(traces_per_bucket);
    }

    // Simple spatial distribution - divide view rect into grid
    int grid_cols = static_cast<int>(std::ceil(std::sqrt(num_buckets)));
    int grid_rows = static_cast<int>(std::ceil(static_cast<double>(num_buckets) / grid_cols));

    double cell_width = world_view_rect.w / grid_cols;
    double cell_height = world_view_rect.h / grid_rows;

    for (const Trace* trace : traces) {
        if (!trace) continue;

        // Get trace center point
        double center_x = (trace->GetStartX() + trace->GetEndX()) * 0.5;
        double center_y = (trace->GetStartY() + trace->GetEndY()) * 0.5;

        // Determine which grid cell this trace belongs to
        int col = static_cast<int>((center_x - world_view_rect.x) / cell_width);
        int row = static_cast<int>((center_y - world_view_rect.y) / cell_height);

        // Clamp to valid range
        col = std::max(0, std::min(col, grid_cols - 1));
        row = std::max(0, std::min(row, grid_rows - 1));

        // Calculate bucket index
        size_t bucket_index = static_cast<size_t>(row * grid_cols + col);
        bucket_index = std::min(bucket_index, num_buckets - 1);

        buckets[bucket_index].push_back(trace);
    }
}

void RenderPipeline::RenderTracesBatchedAsync(BLContext& bl_ctx, 
                                             const std::vector<const Trace*>& traces,
                                             const BLRgba32& color, 
                                             const BLRect& world_view_rect,
                                             BLStrokeCap start_cap, 
                                             BLStrokeCap end_cap, 
                                             double thickness_override) {
    if (traces.empty()) return;
    
    // Set stroke style once for all traces
    bl_ctx.setStrokeStyle(color);
    
    // Create stroke options once
    BLStrokeOptions stroke_options;
    stroke_options.startCap = start_cap;
    stroke_options.endCap = end_cap;
    
    // Batch size tuning - process traces in chunks to improve cache locality
    constexpr size_t BATCH_SIZE = 64;
    
    // Pre-allocate path objects to reduce memory allocations
    std::vector<BLPath> path_batch(std::min(BATCH_SIZE, traces.size()));
    
    // Process traces in batches
    for (size_t i = 0; i < traces.size(); i += BATCH_SIZE) {
        size_t end_idx = std::min(i + BATCH_SIZE, traces.size());
        size_t batch_count = end_idx - i;
        
        // Prepare all paths in batch
        for (size_t j = 0; j < batch_count; j++) {
            const Trace* trace = traces[i + j];
            if (!trace) continue;
            
            // Skip traces outside view rect (culling)
            BLRect trace_bounds = trace->GetBoundingBox();
            if (!AreRectsIntersecting(trace_bounds, world_view_rect)) continue;
            
            // Get or create path
            BLPath& path = path_batch[j];
            path.clear();
            
            // Set thickness for this trace
            double thickness = (thickness_override > 0.0) ? 
                thickness_override : trace->GetWidth();
            stroke_options.width = thickness;
            
            // Create path for this trace
                path.moveTo(trace->GetStartX(), trace->GetStartY());
                path.lineTo(trace->GetEndX(), trace->GetEndY());
        }
        
        // Stroke all paths in batch
        for (size_t j = 0; j < batch_count; j++) {
            // CORRECT: First set stroke options, then stroke with the color already set
            bl_ctx.setStrokeWidth(stroke_options.width);
            bl_ctx.setStrokeStartCap(static_cast<BLStrokeCap>(stroke_options.startCap));  // requires a static cast to BLStrokeCap
            bl_ctx.setStrokeEndCap(static_cast<BLStrokeCap>(stroke_options.endCap));  // requires a static cast to BLStrokeCap
            bl_ctx.strokePath(path_batch[j]);  // Use the color already set above
        }
    }
}

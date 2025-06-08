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

// For logging or error reporting:
// #include <iostream>
// #include <SDL3/SDL_log.h>

// Default values for rendering elements
static constexpr double kDefaultTraceWidth = 0.05;
static constexpr double kMinViaExtent = 0.01;  // Used for AABB culling if radii are zero/negative
static constexpr double kDefaultArcThickness = 0.05;
static constexpr double kDefaultComponentMinDimension = 0.1;

// Placeholder layer IDs - move to a proper constants header or Board.hpp
static constexpr int kSilkscreenLayerId = 17;
static constexpr int kBoardOutlineLayerId = 28;  // Example, adjust as needed

// Forward declaration for use in RenderPin's lambda
static void RenderCapsule(BLContext& ctx, double width, double height, double x_coord, double y_coord, const BLRgba32& fill_color, const BLRgba32& stroke_color);

RenderPipeline::RenderPipeline() : m_render_context_(nullptr), m_initialized_(false)
{
    // std::cout << "RenderPipeline created." << std::endl;
}

RenderPipeline::~RenderPipeline()
{
    // Ensure Shutdown is called if not already.
    if (m_initialized_) {
        // std::cerr << "RenderPipeline destroyed without calling Shutdown() first!" << std::endl;
        Shutdown();
    }
    // std::cout << "RenderPipeline destroyed." << std::endl;
}

bool RenderPipeline::Initialize(RenderContext& context)
{
    m_render_context_ = &context;  // Store pointer to the context
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
}

// Move Camera/Viewport adjustments to their own functions
BLMatrix2D RenderPipeline::ViewMatrix(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport)
{
    BLMatrix2D view_matrix = bl_ctx.metaTransform();
    view_matrix.translate(viewport.GetWidth() / 2.0, viewport.GetHeight() / 2.0);
    view_matrix.scale(camera.GetZoom());
    view_matrix.rotate(-camera.GetRotation() * (static_cast<float>(kPi) / 180.0f));
    view_matrix.translate(-camera.GetPosition().x_ax, -camera.GetPosition().y_ax);
    return view_matrix;
}

// Helper function to check if two AABB rectangles intersect
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

// Helper function to transform an AABB by a matrix
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

        // Performance optimization: Direct layer access instead of iterating all layers
        for (int layer_id : target_layer_ids) {
            auto layer_elements_it = board.m_elements_by_layer.find(layer_id);
            if (layer_elements_it == board.m_elements_by_layer.end())
                continue;

            // Performance optimization: Check layer visibility once per layer
            const Board::LayerInfo* layer_info = board.GetLayerById(layer_id);
            if (!layer_info || !layer_info->IsVisible())
                continue;

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
                    if (!type_matches)
                        continue;
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
                            BLStrokeCap start_cap = BL_STROKE_CAP_ROUND;
                            BLStrokeCap end_cap = BL_STROKE_CAP_ROUND;
                            int net_id = trace->GetNetId();
                            BLPoint current_start_point(trace->GetStartX(), trace->GetStartY());
                            BLPoint current_end_point(trace->GetEndX(), trace->GetEndY());
                            auto net_it = trace_cap_manager.find(net_id);
                            if (net_it != trace_cap_manager.end()) {
                                const auto& prev_endpoints = net_it->second;
                                // Simplified cap logic for brevity; adapt from original if more complex
                                if (ArePointsClose(current_start_point, prev_endpoints.first) || ArePointsClose(current_start_point, prev_endpoints.second)) {
                                    start_cap = BL_STROKE_CAP_ROUND_REV;
                                }
                                if (ArePointsClose(current_end_point, prev_endpoints.first) || ArePointsClose(current_end_point, prev_endpoints.second)) {
                                    end_cap = BL_STROKE_CAP_ROUND_REV;
                                }
                            }

                            // Pass board outline thickness for board outline elements
                            double thickness_override = is_board_outline_pass ? board_outline_thickness : -1.0;
                            RenderTrace(bl_ctx, *trace, adjusted_world_view_rect, start_cap, end_cap, thickness_override);
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
                        if (auto text_label = dynamic_cast<const TextLabel*>(element_ptr.get())) {
                            RenderTextLabel(bl_ctx, *text_label, current_element_color);  // Color already set
                        }
                        break;
                    default:
                        break;
                }
            }
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

    // Performance optimization: Batch component rendering with reduced overhead
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

            Component* component_to_render = dynamic_cast<Component*>(element_ptr.get());
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

            // Performance optimization: Batch selection state checks
            const bool current_component_is_selected_element = (selected_element == component_to_render);
            bool current_component_is_selected_net = false;

            if (selected_net_id != -1) {
                // Performance optimization: Early exit on first match
                for (const auto& pin_ptr : component_to_render->pins) {
                    if (pin_ptr && pin_ptr->GetNetId() == selected_net_id) {
                        current_component_is_selected_net = true;
                        break;
                    }
                }
            }

            // Performance optimization: Use pre-computed colors
            BLRgba32 comp_fill_color, comp_stroke_color;
            if (current_component_is_selected_element) {
                comp_fill_color = comp_stroke_color = selected_element_highlight_color;
            } else if (current_component_is_selected_net) {
                comp_fill_color = comp_stroke_color = highlight_color;
            } else {
                comp_fill_color = component_fill_color;
                comp_stroke_color = component_stroke_color;
            }

            RenderComponent(bl_ctx, *component_to_render, board, adjusted_world_view_rect,
                          comp_fill_color, comp_stroke_color, theme_color_cache, selected_net_id, selected_element);
        }
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

    // Render Component-Specific Text Labels
    for (const auto& label_ptr : component.text_labels) {
        if (label_ptr && label_ptr->IsVisible()) {
            const Board::LayerInfo* element_layer = board.GetLayerById(label_ptr->GetLayerId());
            if (element_layer && element_layer->IsVisible()) {
                // Use layer_id_color_cache for text labels on specific layers
                BLRgba32 text_color = theme_color_cache.count(BoardDataManager::ColorType::kSilkscreen) ? theme_color_cache.at(BoardDataManager::ColorType::kSilkscreen) : BLRgba32(0xFFFFFFFF);
                RenderTextLabel(bl_ctx, *label_ptr, text_color);
            }
        }
    }
}

void RenderPipeline::RenderTextLabel(BLContext& bl_ctx, const TextLabel& text_label, const BLRgba32& color)
{
    if (!text_label.IsVisible() || text_label.text_content.empty())  // Ensure isVisible() is used
    {
        return;
    }

    BLFont font;
    BLFontFace face;
    BLResult err = BL_SUCCESS;

    // Try to find cached font face
    auto it = m_font_face_cache_.find(text_label.font_family);
    if (it != m_font_face_cache_.end()) {
        face = it->second;
        // Optional: Check if face is valid, though it should be if cached.
        // if (!face.isValid()) { /* Handle error or remove from cache */ }
    } else if (!text_label.font_family.empty()) {
        err = face.createFromFile(text_label.font_family.c_str());
        if (err == BL_SUCCESS) {
            m_font_face_cache_[text_label.font_family] = face;  // Cache successfully loaded font
        }
    }

    // If specific font failed or was not provided, or not found in cache and not specified
    if (err != BL_SUCCESS || text_label.font_family.empty() || !face.isValid()) {
        const char* fallbackFonts[] = {"DejaVuSans.ttf", "arial.ttf", "LiberationSans-Regular.ttf"};
        bool loadedFallback = false;
        for (const char* fontName : fallbackFonts) {
            // Check cache first for fallback fonts
            auto fallback_it = m_font_face_cache_.find(fontName);
            if (fallback_it != m_font_face_cache_.end()) {
                face = fallback_it->second;
                if (face.isValid()) {
                    loadedFallback = true;
                    break;
                }
            }
            // If not in cache, try to load and then cache it
            err = face.createFromFile(fontName);
            if (err == BL_SUCCESS) {
                m_font_face_cache_[fontName] = face;  // Cache successfully loaded fallback font
                loadedFallback = true;
                break;
            }
        }
        if (!loadedFallback) {
            // std::cerr << "Failed to load any fallback font for text label: " << text_label.text_content << std::endl;
            return;
        }
    }

    font.createFromFace(face, static_cast<float>(text_label.font_size * text_label.scale));

    bl_ctx.setFillStyle(color);

    if (text_label.rotation != 0.0) {
        bl_ctx.save();
        bl_ctx.translate(text_label.coords.x_ax, text_label.coords.y_ax);
        bl_ctx.rotate(text_label.rotation * (kPi / 180.0));
        // Using fillUtf8Text as per Blend2D documentation
        bl_ctx.fillUtf8Text(BLPoint(0, 0), font, text_label.text_content.c_str());
        bl_ctx.restore();
    } else {
        // Using fillUtf8Text. Adjusted y for baseline approximation.
        bl_ctx.fillUtf8Text(BLPoint(text_label.coords.x_ax, text_label.coords.y_ax + text_label.font_size), font, text_label.text_content.c_str());
    }
}

// Implementation for Process, AddRenderable, Execute, AddStage, etc., if used:
// void RenderPipeline::Process(RenderContext& context, IRenderable* renderable) {
//     if (!m_initialized || !renderable) return;
//     // Simplistic: just call render on the object.
//     // A more complex pipeline would iterate through its stages.
//     // renderable->Render(context, *this);
//     // Or, for each stage:
//     // for (const auto& stage : m_stages) {
//     //     if (stage) stage->Execute(context, *renderable);
//     // }
// }

// void RenderPipeline::Execute(RenderContext& context) {
//     if (!m_initialized) return;
//     BeginPipeline(context);
//     // for (IRenderable* renderable : m_managedRenderables) { // If pipeline manages them
//     //     Process(context, renderable);
//     // }
//     EndPipeline(context);
// }

// ***** Reworking Pin rendering *****
// void RenderPipeline::RenderPin(BLContext &bl_ctx, const Pin &pin, const Component &component, const Board &board, const BLRgba32 &highlightColor)
// {
//     const LayerInfo *layer_info = board.GetLayerById(pin.getLayerId()); // Ensure getLayerId() is used
//     if (!layer_info || !layer_info->IsVisible())
//     {
//         return;
//     }
//     // We use a ternary operator in Component::Render to determine if the pin is on the selected net, and use highlight color if it is, otherwise use layer_info->GetColor
//     bl_ctx.setFillStyle(highlightColor);
//     // DEBUG: If pin.debug_color is set, use it instead of layer_info->GetColor
//     // if (pin.debug_color != BLRgba32(0, 0, 0, 0))
//     // {
//     //     bl_ctx.setFillStyle(pin.debug_color);
//     //     printf("[RenderPin]: Pin %s: Using debug color %d\n", pin.pin_name.c_str(), pin.debug_color);
//     // }
//     // NOTE: The component's main transformation (center_x, center_y, rotation)
//     // is ALREADY APPLIED to bl_ctx by the calling RenderComponent method.
//     // So, pin.x_coord, pin.y_coord, and shape.x_offset/y_offset are treated as local
//     // to the component's origin.

//     std::visit([&](auto &&shape) // Note: auto&& shape is important here for perfect forwarding
//                {
//         using T = std::decay_t<decltype(shape)>;
//         // Pin coordinates are now global, no need for offsets
//         double pin_center_x = pin.x_coord;
//         double pin_center_y = pin.y_coord;

//         if constexpr (std::is_same_v<T, CirclePad>) {
//             bl_ctx.fillCircle(pin_center_x, pin_center_y, shape.radius);
//         } else if constexpr (std::is_same_v<T, RectanglePad>) {
//             double width = pin.width;
//             double height = pin.height;

//             if (pin.orientation == PinOrientation::Vertical) {
//                 // width = pin.initial_width; // Already set
//                 // height = pin.initial_height; // Already set
//             } else if (pin.orientation == PinOrientation::Horizontal) {
//                 width = pin.height; // Swap for horizontal
//                 height = pin.width;
//             } // Else, natural orientation uses initial_width and initial_height as is

//             // Adjust if interior and component aspect ratio suggests a swap
//             if (pin.local_edge == LocalEdge::INTERIOR) {
//                 bool component_taller = component.height > component.width;
//                 bool pin_wider_than_tall = pin.width > pin.height; // Based on initial dimensions

//                 if (component_taller && pin_wider_than_tall) {
//                     // Tall component, but pin is initially wide. Make pin tall.
//                     width = pin.height;
//                     height = pin.width;
//                 } else if (!component_taller && !pin_wider_than_tall) {
//                     // Wide component, but pin is initially tall. Make pin wide.
//                     // This case might be tricky: if pin is initially square, it remains square.
//                     // If pin is (10w, 20h) and component is (100w, 50h), should it become (20w,10h)?
//                     // The original logic implies this swap.
//                     width = pin.height;
//                     height = pin.width;
//                 }
//                 // If component_taller && !pin_wider_than_tall (tall comp, tall pin) -> no change from initial
//                 // If !component_taller && pin_wider_than_tall (wide comp, wide pin) -> no change from initial
//             }

//             // Draw centered rectangle at pin_center_x, pin_center_y
//             bl_ctx.fillRect(BLRect(pin_center_x - width / 2.0, pin_center_y - height / 2.0, width, height));

//         } else if constexpr (std::is_same_v<T, CapsulePad>) {
//             // Effective pad center is pin_center_x, pin_center_y

//             double cap_axis_len; // Length along the capsule's main axis (e.g., height if vertical)
//             double cap_cross_len; // Length across the capsule's main axis (e.g., width if vertical)
//             bool is_vertical;

//             if (pin.orientation == PinOrientation::Natural) {
//                 is_vertical = shape.height > shape.width;
//                 cap_axis_len = is_vertical ? shape.height : shape.width;
//                 cap_cross_len = is_vertical ? shape.width : shape.height;
//             } else if (pin.orientation == PinOrientation::Vertical) {
//                 is_vertical = true;
//                 // If pin is meant to be vertical, its 'height' is shape.height, 'width' is shape.width
//                 // unless shape itself is wider than tall, then we use shape.width as effective height.
//                 cap_axis_len = shape.height >= shape.width ? shape.height : shape.width;
//                 cap_cross_len = shape.height >= shape.width ? shape.width : shape.height;
//             } else { // PinOrientation::Horizontal
//                 is_vertical = false;
//                 // If pin is meant to be horizontal, its 'width' is shape.width, 'height' is shape.height
//                 // unless shape itself is taller than wide, then we use shape.height as effective width.
//                 cap_axis_len = shape.width >= shape.height ? shape.width : shape.height;
//                 cap_cross_len = shape.width >= shape.height ? shape.height : shape.width;
//             }

//             BLPath path;
//             if (is_vertical) {
//                 double radius = cap_cross_len / 2.0;
//                 double rect_height = cap_axis_len - cap_cross_len; // cap_axis_len is total height
//                 if (rect_height < 0) rect_height = 0;

//                 path.moveTo(pin_center_x - radius, pin_center_y - rect_height / 2.0);
//                 path.arcTo(pin_center_x, pin_center_y - rect_height / 2.0, radius, radius, kPi, kPi); // Top cap
//                 path.lineTo(pin_center_x + radius, pin_center_y + rect_height / 2.0);
//                 path.arcTo(pin_center_x, pin_center_y + rect_height / 2.0, radius, radius, 0.0, kPi); // Bottom cap
//                 path.close();
//             } else { // Horizontal Capsule
//                 double radius = cap_cross_len / 2.0;
//                 double rect_width = cap_axis_len - cap_cross_len; // cap_axis_len is total width
//                 if (rect_width < 0) rect_width = 0;

//                 path.moveTo(pin_center_x - rect_width / 2.0, pin_center_y - radius);
//                 path.lineTo(pin_center_x + rect_width / 2.0, pin_center_y - radius);
//                 path.arcTo(pin_center_x + rect_width / 2.0, pin_center_y, radius, radius, -kPi / 2.0, kPi); // Right cap
//                 path.lineTo(pin_center_x - rect_width / 2.0, pin_center_y + radius);
//                 path.arcTo(pin_center_x - rect_width / 2.0, pin_center_y, radius, radius, kPi / 2.0, kPi); // Left cap
//                 path.close();
//             }
//             bl_ctx.fillPath(path);
//         } }, pin.pad_shape);
// }
// Render a pin using Blend2D, automatically handling all shape types
void RenderPipeline::RenderPin(BLContext& ctx, const Pin& pin, const Component* parent_component, const BLRgba32& fill_color, const BLRgba32& stroke_color, const Board& board)
{
    // Get pin dimensions (these are in the pin's local coordinate system)
    auto [pin_width, pin_height] = pin.GetDimensions();  // Should return dimensions pre-rotation
    double x_coord = pin.coords.x_ax;
    double y_coord = pin.coords.y_ax;
    double rotation = pin.rotation;

    // Performance optimization: Remove debug logging in production builds
    #ifdef DEBUG_PIN_ROTATION
    static int debug_count = 0;
    if (debug_count < 5) {  // Reduced debug output
        std::cout << "Pin rotation debug: " << rotation << "° (W=" << pin_width << ", H=" << pin_height << ")" << std::endl;
        debug_count++;
    }
    #endif

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

        // Performance optimization: Remove debug logging in production builds
        #ifdef DEBUG_PIN_ROTATION
        static int transform_debug_count = 0;
        if (transform_debug_count < 5) {
            std::cout << "  Applying rotation: " << rotation << "° at (" << x_coord << ", " << y_coord << ")" << std::endl;
            transform_debug_count++;
        }
        #endif

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


// ctx.restore(); // DO NOT RESTORE HERE. THIS BREAKS THE RENDERING OF PINS. LEAVE IT COMMENTED, SO WE KNOW NOT TO RESTORE HERE.

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

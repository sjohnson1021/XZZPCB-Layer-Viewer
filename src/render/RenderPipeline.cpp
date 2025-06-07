#include "render/RenderPipeline.hpp"

#include <algorithm>  // For std::min/max for AABB checks
#include <cmath>      // For std::cos and std::sin
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
    bl_ctx.save();
    bl_ctx.applyTransform(ViewMatrix(bl_ctx, camera, viewport));

    std::unordered_map<BoardDataManager::ColorType, BLRgba32> theme_color_cache;
    std::unordered_map<int, BLRgba32> layer_id_color_cache;
    int selected_net_id = -1;
    BoardDataManager::BoardSide current_view_side = BoardDataManager::BoardSide::kBoth;

    // Populate color caches, selected_net_id, and current view side
    if (m_render_context_ && m_render_context_->GetBoardDataManager()) {
        auto bdm = m_render_context_->GetBoardDataManager();
        selected_net_id = bdm->GetSelectedNetId();
        current_view_side = bdm->GetCurrentViewSide();
        theme_color_cache[BoardDataManager::ColorType::kNetHighlight] = bdm->GetColor(BoardDataManager::ColorType::kNetHighlight);
        theme_color_cache[BoardDataManager::ColorType::kComponentFill] = bdm->GetColor(BoardDataManager::ColorType::kComponentFill);
        theme_color_cache[BoardDataManager::ColorType::kComponentStroke] = bdm->GetColor(BoardDataManager::ColorType::kComponentStroke);
        theme_color_cache[BoardDataManager::ColorType::kPinFill] = bdm->GetColor(BoardDataManager::ColorType::kPinFill);
        theme_color_cache[BoardDataManager::ColorType::kPinStroke] = bdm->GetColor(BoardDataManager::ColorType::kPinStroke);
        theme_color_cache[BoardDataManager::ColorType::kBaseLayer] = bdm->GetColor(BoardDataManager::ColorType::kBaseLayer);
        theme_color_cache[BoardDataManager::ColorType::kSilkscreen] = bdm->GetColor(BoardDataManager::ColorType::kSilkscreen);
        theme_color_cache[BoardDataManager::ColorType::kBoardEdges] = bdm->GetColor(BoardDataManager::ColorType::kBoardEdges);

        const auto& board_layers = board.GetLayers();  // Renamed to avoid conflict with loop var
        for (const Board::LayerInfo& layer_info_entry : board_layers) {
            layer_id_color_cache[layer_info_entry.GetId()] = bdm->GetLayerColor(layer_info_entry.GetId());
        }
    }

    // Visual mirror transformation removed - actual element coordinates are now updated
    // when board flip state changes, so no runtime visual transformation is needed
    BLRect adjusted_world_view_rect = world_view_rect;
    // Fallback colors (ensure all keys used below are present in theme_color_cache or have fallbacks)
    BLRgba32 fallback_color(0xFFFF0000);  // Red
    BLRgba32 highlight_color =
        theme_color_cache.count(BoardDataManager::ColorType::kNetHighlight) ? theme_color_cache.at(BoardDataManager::ColorType::kNetHighlight) : BLRgba32(0xFFFFFF00);  // Yellow fallback for highlight
    BLRgba32 component_fill_color =
        theme_color_cache.count(BoardDataManager::ColorType::kComponentFill) ? theme_color_cache.at(BoardDataManager::ColorType::kComponentFill) : BLRgba32(0xFF007BFF);  // Blue fallback for component
    BLRgba32 component_stroke_color = 
		theme_color_cache.count(BoardDataManager::ColorType::kComponentStroke) ? theme_color_cache.at(BoardDataManager::ColorType::kComponentStroke) : BLRgba32(0xFF000000);  // Black fallback for component
    
	BLRgba32 pin_fill_color =
        theme_color_cache.count(BoardDataManager::ColorType::kPinFill) ? theme_color_cache.at(BoardDataManager::ColorType::kPinFill) : BLRgba32(0xC0999999);  // Grey fallback for pin fill
    BLRgba32 pin_stroke_color =
        theme_color_cache.count(BoardDataManager::ColorType::kPinStroke) ? theme_color_cache.at(BoardDataManager::ColorType::kPinStroke) : BLRgba32(0xC0000000);  // Black fallback for pin stroke
	BLRgba32 base_layer_theme_color = theme_color_cache.count(BoardDataManager::ColorType::kBaseLayer) ? theme_color_cache.at(BoardDataManager::ColorType::kBaseLayer) : fallback_color;
    BLRgba32 silkscreen_theme_color =
        theme_color_cache.count(BoardDataManager::ColorType::kSilkscreen) ? theme_color_cache.at(BoardDataManager::ColorType::kSilkscreen) : BLRgba32(0xFFFFFFFF);  // White fallback for silkscreen
    BLRgba32 board_edges_theme_color =
        theme_color_cache.count(BoardDataManager::ColorType::kBoardEdges) ? theme_color_cache.at(BoardDataManager::ColorType::kBoardEdges) : BLRgba32(0xFF00FF00);  // Green fallback for board edges

    // Helper lambda to render elements on specified layers and of specified types
    auto executeRenderPass = [&](const std::vector<int>& target_layer_ids,
                                 const std::vector<ElementType>& target_element_types,
                                 std::map<int, std::pair<BLPoint, BLPoint>>& trace_cap_manager,  // Pass specific map for trace caps
                                 bool is_silkscreen_pass = false,
                                 bool is_board_outline_pass = false) {  // Flags for special color handling
        const auto& all_board_layers = board.GetLayers();
        for (const auto& layer_info : all_board_layers) {
            if (!layer_info.IsVisible())
                continue;

            bool layer_matches = false;
            for (int target_id : target_layer_ids) {
                if (layer_info.GetId() == target_id) {
                    layer_matches = true;
                    break;
                }
            }
            if (!layer_matches && !target_layer_ids.empty())
                continue;  // If target_layer_ids is empty, process all visible layers (for type-based pass)

            auto layer_elements_it = board.m_elements_by_layer.find(layer_info.GetId());
            if (layer_elements_it == board.m_elements_by_layer.end())
                continue;

            const auto& elements_on_layer = layer_elements_it->second;
            for (const auto& element_ptr : elements_on_layer) {
                if (!element_ptr || !element_ptr->IsVisible())
                    continue;

                ElementType current_type = element_ptr->GetElementType();
                bool type_matches = false;
                if (target_element_types.empty()) {  // Empty means render all types on the matched layer(s)
                    type_matches = true;
                } else {
                    for (ElementType target_type : target_element_types) {
                        if (current_type == target_type) {
                            type_matches = true;
                            break;
                        }
                    }
                }
                if (!type_matches)
                    continue;

                bool is_selected_net = (selected_net_id != -1 && element_ptr->GetNetId() == selected_net_id);
                BLRgba32 current_element_color;

                if (is_selected_net) {
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
                    current_element_color = layer_id_color_cache.count(layer_info.GetId()) ? layer_id_color_cache.at(layer_info.GetId()) : base_layer_theme_color;
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
                            RenderTrace(bl_ctx, *trace, adjusted_world_view_rect, start_cap, end_cap);
                            trace_cap_manager[net_id] = {current_start_point, current_end_point};
                        }
                        break;
                    case ElementType::kArc:
                        if (auto arc = dynamic_cast<const Arc*>(element_ptr.get())) {
                            RenderArc(bl_ctx, *arc, adjusted_world_view_rect);
                        }
                        break;
                    case ElementType::kVia:
                        if (auto via = dynamic_cast<const Via*>(element_ptr.get())) {
                            BLRgba32 via_color_from;
                            BLRgba32 via_color_to;
                            if (is_selected_net) {
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
	// -- Bottom Side Pins and Components
	
    // Conditional layer rendering order based on board viewing side for realistic depth perception
    std::vector<int> copper_layer_ids;
    if (current_view_side == BoardDataManager::BoardSide::kBottom) {
        // Bottom side view: Render layers 16→1 (bottom-up order) for proper depth perception
        for (int i = 16; i >= 1; --i)
            copper_layer_ids.push_back(i);
    } else {
        // Top side view (default): Render layers 1→16 (top-down order)
        for (int i = 1; i <= 16; ++i)
            copper_layer_ids.push_back(i);
    }
    std::map<int, std::pair<BLPoint, BLPoint>> trace_cap_manager_copper;
    executeRenderPass(copper_layer_ids, {ElementType::kTrace, ElementType::kArc, ElementType::kVia}, trace_cap_manager_copper);

    std::map<int, std::pair<BLPoint, BLPoint>> trace_cap_manager_silkscreen;  // If traces/arcs can be on silkscreen
    executeRenderPass({kSilkscreenLayerId}, {} /* all types */, trace_cap_manager_silkscreen, true /*is_silkscreen_pass*/);

    std::vector<int> other_trace_layer_ids;
    for (int i = 18; i <= 27; ++i)
        other_trace_layer_ids.push_back(i);
    std::map<int, std::pair<BLPoint, BLPoint>> trace_cap_manager_other;
    executeRenderPass(other_trace_layer_ids, {ElementType::kTrace, ElementType::kArc, ElementType::kVia}, trace_cap_manager_other);

    std::map<int, std::pair<BLPoint, BLPoint>> trace_cap_manager_board_outline;  // If traces/arcs can be on board outline
    executeRenderPass({kBoardOutlineLayerId}, {} /* all types */, trace_cap_manager_board_outline, false, true /*is_board_outline_pass*/);

    // Pass for Components (which includes their pins)
    // Handle both top and bottom component layers separately for proper rendering order
    std::vector<int> component_layer_ids = {Board::kTopCompLayer, Board::kBottomCompLayer};

    for (int comp_layer_id : component_layer_ids) {
        auto comp_layer_elements_it = board.m_elements_by_layer.find(comp_layer_id);
        if (comp_layer_elements_it == board.m_elements_by_layer.end()) {
            continue;  // This layer doesn't exist or has no elements
        }

        const auto& elements_on_comp_layer = comp_layer_elements_it->second;
        for (const auto& element_ptr : elements_on_comp_layer) {
            if (element_ptr && element_ptr->GetElementType() == ElementType::kComponent && element_ptr->IsVisible()) {
                Component* component_to_render = dynamic_cast<Component*>(element_ptr.get());
                if (!component_to_render) {
                    continue;
                }

                // Filter components based on current view side
                bool should_render_component = true;
                if (current_view_side != BoardDataManager::BoardSide::kBoth) {
                    if (current_view_side == BoardDataManager::BoardSide::kTop && component_to_render->side != MountingSide::kTop) {
                        should_render_component = false;
                    } else if (current_view_side == BoardDataManager::BoardSide::kBottom && component_to_render->side != MountingSide::kBottom) {
                        should_render_component = false;
                    }
                }

                if (!should_render_component) {
                    continue;  // Skip this component based on view side filter
                }

                bool current_component_is_selected = false;  // Initialize for this specific component
                if (selected_net_id != -1) {
                    for (const auto& pin_ptr : component_to_render->pins) {
                        if (pin_ptr && pin_ptr->GetNetId() == selected_net_id) {
                            current_component_is_selected = true;
                            break;
                        }
                    }
                }

                BLRgba32 comp_fill_color = current_component_is_selected ? highlight_color : component_fill_color;
				BLRgba32 comp_stroke_color = current_component_is_selected ? highlight_color : component_stroke_color;
                RenderComponent(bl_ctx, *component_to_render, board, adjusted_world_view_rect, comp_fill_color, comp_stroke_color, theme_color_cache, selected_net_id);
            }
        }
    }
    bl_ctx.restore();
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

void RenderPipeline::RenderTrace(BLContext& bl_ctx, const Trace& trace, const BLRect& world_view_rect, BLStrokeCap start_cap, BLStrokeCap end_cap)
{
    // AABB for the trace line segment
    double min_x = std::min(trace.GetStartX(), trace.GetEndX());
    double max_x = std::max(trace.GetStartX(), trace.GetEndX());
    double min_y = std::min(trace.GetStartY(), trace.GetEndY());
    double max_y = std::max(trace.GetStartY(), trace.GetEndY());
    double width_for_aabb = trace.GetWidth() > 0 ? trace.GetWidth() : kDefaultTraceWidth;

    BLRect trace_aabb(min_x - width_for_aabb / 2.0, min_y - width_for_aabb / 2.0, max_x - min_x + width_for_aabb, max_y - min_y + width_for_aabb);

    if (!AreRectsIntersecting(trace_aabb, world_view_rect)) {
        return;  // Cull this trace
    }

    // Color is set by RenderBoard.
    double width = trace.GetWidth();
    if (width <= 0) {
        width = kDefaultTraceWidth;  // Default width. TODO: Configurable or dynamic based on zoom.
    }

    bl_ctx.setStrokeWidth(width);
    bl_ctx.setStrokeStartCap(start_cap);
    bl_ctx.setStrokeEndCap(end_cap);
    bl_ctx.setStrokeJoin(BL_STROKE_JOIN_ROUND);  // Keep round joins for now
    bl_ctx.strokeLine(trace.GetStartX(), trace.GetStartY(), trace.GetEndX(), trace.GetEndY());
}

void RenderPipeline::RenderVia(BLContext& bl_ctx, const Via& via, const Board& board, const BLRect& world_view_rect, const BLRgba32& color_from, const BLRgba32& color_to)
{
    // AABB for the entire via (bounding box of all its pads)
    double max_radius = std::max(via.GetPadRadiusFrom(), via.GetPadRadiusTo());
    if (max_radius <= 0)
        max_radius = kMinViaExtent;  // Ensure some extent for culling if radii are zero/negative

    BLRect via_aabb(via.GetX() - max_radius, via.GetY() - max_radius, 2 * max_radius, 2 * max_radius);

    if (!AreRectsIntersecting(via_aabb, world_view_rect)) {
        return;  // Cull this via
    }

    // Pad on 'from' layer
    const Board::LayerInfo* layer_from_props = board.GetLayerById(via.GetLayerFrom());
    if (layer_from_props && layer_from_props->IsVisible() && via.GetPadRadiusFrom() > 0) {
        // Individual pad culling (could be redundant if via_aabb already culled, but good for precision)
        BLRect pad_from_aabb(via.GetX() - via.GetPadRadiusFrom(), via.GetY() - via.GetPadRadiusFrom(), 2 * via.GetPadRadiusFrom(), 2 * via.GetPadRadiusFrom());
        if (AreRectsIntersecting(pad_from_aabb, world_view_rect)) {
            bl_ctx.setFillStyle(color_from);  // Use cached color_from
            bl_ctx.fillCircle(via.GetX(), via.GetY(), via.GetPadRadiusFrom());
        }
    }

    // Pad on 'to' layer
    const Board::LayerInfo* layer_to_props = board.GetLayerById(via.GetLayerTo());
    if (layer_to_props && layer_to_props->IsVisible() && via.GetPadRadiusTo() > 0) {
        BLRect pad_to_aabb(via.GetX() - via.GetPadRadiusTo(), via.GetY() - via.GetPadRadiusTo(), 2 * via.GetPadRadiusTo(), 2 * via.GetPadRadiusTo());
        if (AreRectsIntersecting(pad_to_aabb, world_view_rect)) {
            bl_ctx.setFillStyle(color_to);  // Use cached color_to
            bl_ctx.fillCircle(via.GetX(), via.GetY(), via.GetPadRadiusTo());
        }
    }
    // Drill hole rendering can be added here if desired.
}

void RenderPipeline::RenderArc(BLContext& bl_ctx, const Arc& arc, const BLRect& world_view_rect)
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
    double thickness = arc.GetThickness();
    if (thickness <= 0) {
        thickness = kDefaultArcThickness;  // Default thickness.
    }
    bl_ctx.setStrokeWidth(thickness);
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
                                     int selected_net_id)
{
    // Calculate component's world AABB accounting for rotation
    double comp_w = component.width;
    double comp_h = component.height;
    double comp_cx = component.center_x;
    double comp_cy = component.center_y;
    double comp_rot_rad = component.rotation * (kPi / 180.0);
    double cos_r = std::cos(comp_rot_rad);
    double sin_r = std::sin(comp_rot_rad);

    // Ensure minimum dimensions for AABB calculation if width/height are zero or negative
    if (comp_w <= 0)
        comp_w = kDefaultComponentMinDimension;
    if (comp_h <= 0)
        comp_h = kDefaultComponentMinDimension;

    // Local corners (relative to component's local origin 0,0 before rotation/translation)
    BLPoint local_corners[4] = {{-comp_w / 2.0, -comp_h / 2.0}, {comp_w / 2.0, -comp_h / 2.0}, {comp_w / 2.0, comp_h / 2.0}, {-comp_w / 2.0, comp_h / 2.0}};

    BLPoint world_corners[4];
    for (int i = 0; i < 4; ++i) {
        // Rotate
        double rx = local_corners[i].x * cos_r - local_corners[i].y * sin_r;
        double ry = local_corners[i].x * sin_r + local_corners[i].y * cos_r;
        // Translate
        world_corners[i].x = rx + comp_cx;
        world_corners[i].y = ry + comp_cy;
    }

    // Find min/max of world_corners to form AABB
    double min_wx = world_corners[0].x, max_wx = world_corners[0].x;
    double min_wy = world_corners[0].y, max_wy = world_corners[0].y;
    for (int i = 1; i < 4; ++i) {
        min_wx = std::min(min_wx, world_corners[i].x);
        max_wx = std::max(max_wx, world_corners[i].x);
        min_wy = std::min(min_wy, world_corners[i].y);
        max_wy = std::max(max_wy, world_corners[i].y);
    }
    BLRect component_world_aabb(min_wx, min_wy, max_wx - min_wx, max_wy - min_wy);
    if (!AreRectsIntersecting(component_world_aabb, world_view_rect)) {
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
    bl_ctx.setStrokeWidth(0.1);  // Thin outline
    bl_ctx.strokePath(outline);

    // Render Component Pins
    BLRgba32 pin_highlight_for_pins =
        theme_color_cache.count(BoardDataManager::ColorType::kNetHighlight) ? theme_color_cache.at(BoardDataManager::ColorType::kNetHighlight) : BLRgba32(0xFFFFFFFF);  // Consistent highlight fallback
    BLRgba32 pin_fill_color = theme_color_cache.count(BoardDataManager::ColorType::kPinFill) ? theme_color_cache.at(BoardDataManager::ColorType::kPinFill): BLRgba32(0xC0999999);  // Consistent pin theme fallback (from RenderBoard)
    BLRgba32 pin_stroke_color = theme_color_cache.count(BoardDataManager::ColorType::kPinStroke) ? theme_color_cache.at(BoardDataManager::ColorType::kPinStroke) : BLRgba32(0xC0000000);  // Consistent pin stroke fallback (from RenderBoard)
	
    for (const auto& pin_ptr : component.pins) {
        if (pin_ptr && pin_ptr->IsVisible()) {
            bool is_pin_selected_net = (selected_net_id != -1 && pin_ptr->GetNetId() == selected_net_id);
            BLRgba32 final_pin_color = is_pin_selected_net ? pin_highlight_for_pins : pin_fill_color;
			BLRgba32 final_pin_stroke_color = is_pin_selected_net ? pin_highlight_for_pins : pin_stroke_color;
            RenderPin(bl_ctx, *pin_ptr, &component, final_pin_color, final_pin_stroke_color);
        }
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
void RenderPipeline::RenderPin(BLContext& ctx, const Pin& pin, const Component* parent_component, const BLRgba32& fill_color, const BLRgba32& stroke_color)
{
    // Get pin dimensions (these are in the pin's local coordinate system)
    auto [pin_width, pin_height] = pin.GetDimensions();  // Should return dimensions pre-rotation
    double x_coord = pin.coords.x_ax;
    double y_coord = pin.coords.y_ax;
    double rotation = pin.rotation;

    // Debug logging for pin rotation (temporary)
    static int debug_count = 0;
    if (debug_count < 20) {  // Limit debug output
        std::cout << "Pin rotation debug: " << rotation << "° (W=" << pin_width << ", H=" << pin_height << ")" << std::endl;
        debug_count++;
    }

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

        // Debug: Log the transformation being applied
        if (debug_count < 20) {
            std::cout << "  Applying rotation: " << rotation << "° at (" << x_coord << ", " << y_coord << ")" << std::endl;
        }

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

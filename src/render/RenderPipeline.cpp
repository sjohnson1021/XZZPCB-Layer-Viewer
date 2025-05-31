#include "render/RenderPipeline.hpp"
#include "render/RenderContext.hpp" // For access to RenderContext if needed
#include "pcb/Board.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "view/Grid.hpp"              // For Grid class definition for RenderGrid
#include "pcb/elements/Trace.hpp"     // Added for Trace element
#include "pcb/elements/Via.hpp"       // Added for Via element
#include "pcb/elements/Arc.hpp"       // Added for Arc element
#include "pcb/elements/Component.hpp" // Added for Component
#include "pcb/elements/TextLabel.hpp" // Added for TextLabel
#include "pcb/elements/Pin.hpp"       // For Pin and PadShape when rendering pins
#include <blend2d.h>
#include <iostream>  // For logging
#include <iomanip>   // For std::hex/std::dec output manipulator
#include <algorithm> // For std::min/max for AABB checks
#include <cmath>     // For std::cos and std::sin

// For logging or error reporting:
// #include <iostream>
// #include <SDL3/SDL_log.h>

RenderPipeline::RenderPipeline() : m_renderContext(nullptr), m_initialized(false)
{
    // std::cout << "RenderPipeline created." << std::endl;
}

RenderPipeline::~RenderPipeline()
{
    // Ensure Shutdown is called if not already.
    if (m_initialized)
    {
        // std::cerr << "RenderPipeline destroyed without calling Shutdown() first!" << std::endl;
        Shutdown();
    }
    // std::cout << "RenderPipeline destroyed." << std::endl;
}

bool RenderPipeline::Initialize(RenderContext &context)
{
    m_renderContext = &context; // Store pointer to the context
    // Initialize any pipeline-specific resources if needed.
    // For now, it mainly relies on the context passed during Execute.
    std::cout << "RenderPipeline initialized." << std::endl;
    m_initialized = true;
    return true;
}

void RenderPipeline::Shutdown()
{
    // Release any pipeline-specific resources.
    m_renderContext = nullptr;
    // Release all pipeline resources, clear stages.
    // for (auto& stage : m_stages) {
    //     if (stage) stage->Shutdown();
    // }
    // m_stages.clear();
    m_fontFaceCache.clear();

    std::cout << "RenderPipeline shutdown." << std::endl;
    m_initialized = false;
}

void RenderPipeline::BeginScene(BLContext &bl_ctx)
{
    if (!m_initialized)
        return;
    // This method is called by PcbRenderer.
    // It can be used to set up any per-scene state on the bl_ctx if necessary.
    // For now, PcbRenderer->BeginFrame() clears the target, which is sufficient.
    // bl_ctx.save(); // Example: save context state if pipeline makes many changes
    // std::cout << "RenderPipeline: Beginning scene." << std::endl;
}

void RenderPipeline::EndScene()
{
    if (!m_initialized)
        return;
    // Called by PcbRenderer after Execute.
    // Can be used to restore context state if saved in BeginScene.
    // if (m_renderContext && m_renderContext->GetBlend2DContext().isActive()) { // Check if context is valid
    //     m_renderContext->GetBlend2DContext().restore(); // Example: restore context state
    // }
    // std::cout << "RenderPipeline: Ending scene." << std::endl;
}

void RenderPipeline::Execute(
    BLContext &bl_ctx,
    const Board *board,
    const Camera &camera,
    const Viewport &viewport,
    const Grid &grid,
    bool shouldRenderGrid, // Parameter name matching declaration
    bool shouldRenderBoard // Parameter name matching declaration
)
{
    if (!m_initialized)
    {
        std::cerr << "RenderPipeline::Execute Error: Not initialized." << std::endl;
        return;
    }

    // Conditionally render the grid
    if (shouldRenderGrid)
    {
        RenderGrid(bl_ctx, camera, viewport, grid);
    }

    // Conditionally render the board
    if (shouldRenderBoard && board)
    {
        BLRect worldViewRect = GetVisibleWorldBounds(camera, viewport); // Calculate once
        RenderBoard(bl_ctx, *board, camera, viewport, worldViewRect);   // Pass to RenderBoard
    }
}
// Helper function to check if two AABB rectangles intersect
bool AreRectsIntersecting(const BLRect &r1, const BLRect &r2)
{
    return !(r1.x + r1.w < r2.x || // r1 is to the left of r2
             r1.x > r2.x + r2.w || // r1 is to the right of r2
             r1.y + r1.h < r2.y || // r1 is above r2
             r1.y > r2.y + r2.h);  // r1 is below r2
}

// Helper function to transform an AABB by a matrix
BLRect TransformAABB(const BLRect &localAABB, const BLMatrix2D &transform)
{
    BLPoint p1 = {localAABB.x, localAABB.y};
    BLPoint p2 = {localAABB.x + localAABB.w, localAABB.y};
    BLPoint p3 = {localAABB.x, localAABB.y + localAABB.h};
    BLPoint p4 = {localAABB.x + localAABB.w, localAABB.y + localAABB.h};

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

BLRect RenderPipeline::GetVisibleWorldBounds(const Camera &camera, const Viewport &viewport) const
{
    // This logic is similar to Grid::GetVisibleWorldBounds
    Vec2 screenCorners[4] = {
        {static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY())},
        {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY())},
        {static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY() + viewport.GetHeight())},
        {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY() + viewport.GetHeight())}};

    Vec2 worldMin = viewport.ScreenToWorld(screenCorners[0], camera);
    Vec2 worldMax = worldMin;

    for (int i = 1; i < 4; ++i)
    {
        Vec2 worldCorner = viewport.ScreenToWorld(screenCorners[i], camera);
        worldMin.x = std::min(worldMin.x, worldCorner.x);
        worldMin.y = std::min(worldMin.y, worldCorner.y);
        worldMax.x = std::max(worldMax.x, worldCorner.x);
        worldMax.y = std::max(worldMax.y, worldCorner.y);
    }
    // worldMin and worldMax are now correctly in Y-Down world coordinates.
    // BLRect expects (x, y, width, height) where x,y is the top-left corner.
    // In Y-Down, the top-left y is worldMin.y.
    return BLRect(worldMin.x, worldMin.y, worldMax.x - worldMin.x, worldMax.y - worldMin.y);
}

void RenderPipeline::RenderBoard(
    BLContext &bl_ctx,
    const Board &board,
    const Camera &camera,
    const Viewport &viewport,
    const BLRect &worldViewRect) // Added worldViewRect parameter
{
    bl_ctx.save();

    // Apply viewport and camera transformations
    bl_ctx.translate(viewport.GetWidth() / 2.0, viewport.GetHeight() / 2.0);
    bl_ctx.scale(camera.GetZoom());
    bl_ctx.rotate(-camera.GetRotation() * (static_cast<float>(BL_M_PI) / 180.0f));
    bl_ctx.translate(-camera.GetPosition().x, -camera.GetPosition().y);

    // Get the selected net ID from BoardDataManager
    int selectedNetId = -1;
    BLRgba32 highlightColor;
    BLRgba32 baseColor;
    if (m_renderContext && m_renderContext->GetBoardDataManager())
    {
        selectedNetId = m_renderContext->GetBoardDataManager()->GetSelectedNetId();
        highlightColor = m_renderContext->GetBoardDataManager()->GetColor(BoardDataManager::ColorType::kNetHighlight);
        baseColor = m_renderContext->GetBoardDataManager()->GetColor(BoardDataManager::ColorType::kBaseLayer);
    }

    // Iterate through defined layers to maintain drawing order and visibility
    for (const LayerInfo &layerInfo : board.GetLayers()) // board.GetLayers() provides the correct order
    {
        if (!layerInfo.IsVisible())
            continue;

        // Find elements associated with the current layer ID in m_elementsByLayer
        auto it = board.m_elementsByLayer.find(layerInfo.GetId());
        if (it != board.m_elementsByLayer.end())
        {
            const auto &elements_on_layer = it->second;
            for (const auto &element_ptr : elements_on_layer)
            {
                if (!element_ptr || !element_ptr->isVisible())
                    continue;

                bool isSelectedNet = (selectedNetId != -1 && element_ptr->getNetId() == selectedNetId);
                BLRgba32 current_element_color;

                ElementType type = element_ptr->getElementType();

                if (isSelectedNet)
                {
                    current_element_color = highlightColor;
                }
                else if (type == ElementType::COMPONENT)
                {
                    current_element_color = m_renderContext->GetBoardDataManager()->GetColor(BoardDataManager::ColorType::kComponent);
                }
                else if (type == ElementType::PIN)
                {
                    current_element_color = m_renderContext->GetBoardDataManager()->GetColor(BoardDataManager::ColorType::kPin);
                }
                else
                {
                    current_element_color = m_renderContext->GetBoardDataManager()->GetLayerColor(layerInfo.GetId());
                }

                bl_ctx.setStrokeStyle(current_element_color);
                bl_ctx.setFillStyle(current_element_color);

                switch (type)
                {
                case ElementType::COMPONENT:
                    if (auto component = dynamic_cast<const Component *>(element_ptr.get()))
                    {
                        // Color already set above by current_element_color
                        RenderComponent(bl_ctx, *component, board, worldViewRect, current_element_color);
                    }
                    break;

                case ElementType::PIN:
                    if (auto pin = dynamic_cast<const Pin *>(element_ptr.get()))
                    {
                        // This case is for standalone pins not part of a component from m_elementsByLayer.
                        // If pins are ALWAYS part of components and not in m_elementsByLayer, this might not be hit.
                        // Color already set above by current_element_color.
                        // We need to determine if this pin is part of any component to pass to RenderPin if its API strictly requires it.
                        // For now, assuming RenderPin can handle a nullptr for component if it's a standalone pin.
                        RenderPin(bl_ctx, *pin, nullptr, current_element_color);
                    }
                    break;

                case ElementType::TRACE:
                    if (auto trace = dynamic_cast<const Trace *>(element_ptr.get()))
                    {
                        RenderTrace(bl_ctx, *trace, worldViewRect);
                    }
                    break;
                case ElementType::ARC:
                    if (auto arc = dynamic_cast<const Arc *>(element_ptr.get()))
                    {
                        RenderArc(bl_ctx, *arc, worldViewRect);
                    }
                    break;
                case ElementType::VIA:
                    if (auto via = dynamic_cast<const Via *>(element_ptr.get()))
                    {
                        RenderVia(bl_ctx, *via, board, worldViewRect);
                    }
                    break;
                case ElementType::TEXT_LABEL:
                    if (auto textLabel = dynamic_cast<const TextLabel *>(element_ptr.get()))
                    {
                        RenderTextLabel(bl_ctx, *textLabel, current_element_color);
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    // Render Components separately.
    for (const auto &component : board.GetComponents())
    {
        const LayerInfo *componentLayerInfo = board.GetLayerById(component.layer);
        if (componentLayerInfo && componentLayerInfo->IsVisible())
        {
            bool componentHasSelectedNet = false;
            if (selectedNetId != -1)
            {
                for (const auto &pin : component.pins)
                {
                    if (pin && pin->getNetId() == selectedNetId)
                    {
                        componentHasSelectedNet = true;
                        break;
                    }
                }
            }

            BLRgba32 component_draw_color = componentHasSelectedNet ? highlightColor
                                                                    : m_renderContext->GetBoardDataManager()->GetColor(BoardDataManager::ColorType::kComponent);
            RenderComponent(bl_ctx, component, board, worldViewRect, component_draw_color);
        }
    }

    bl_ctx.restore();
}

void RenderPipeline::RenderGrid(
    BLContext &bl_ctx,
    const Camera &camera,
    const Viewport &viewport,
    const Grid &grid)
{
    try
    {
        // Save Blend2D context state before grid rendering
        bl_ctx.save();

        // Call the Grid's own render method, providing the Blend2D context.
        grid.Render(bl_ctx, camera, viewport);

        // Restore Blend2D context state after grid rendering
        bl_ctx.restore();
    }
    catch (const std::exception &e)
    {
        std::cerr << "RenderPipeline::RenderGrid Exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "RenderPipeline::RenderGrid: Unknown exception during grid rendering" << std::endl;
    }
}

// Private helper methods for rendering specific PCB elements
// These would typically be declared in RenderPipeline.hpp if it were being modified here.

void RenderPipeline::RenderTrace(BLContext &bl_ctx, const Trace &trace, const BLRect &worldViewRect)
{
    // AABB for the trace line segment
    double min_x = std::min(trace.GetStartX(), trace.GetEndX());
    double max_x = std::max(trace.GetStartX(), trace.GetEndX());
    double min_y = std::min(trace.GetStartY(), trace.GetEndY());
    double max_y = std::max(trace.GetStartY(), trace.GetEndY());
    double width_for_aabb = trace.GetWidth() > 0 ? trace.GetWidth() : 0.05;

    BLRect trace_aabb(min_x - width_for_aabb / 2.0, min_y - width_for_aabb / 2.0,
                      max_x - min_x + width_for_aabb, max_y - min_y + width_for_aabb);

    if (!AreRectsIntersecting(trace_aabb, worldViewRect))
    {
        return; // Cull this trace
    }

    // Color is set by RenderBoard.
    double width = trace.GetWidth();
    if (width <= 0)
    {
        width = 0.05; // Default width. TODO: Configurable or dynamic based on zoom.
    }

    bl_ctx.setStrokeWidth(width);
    bl_ctx.setStrokeStartCap(BL_STROKE_CAP_ROUND);
    bl_ctx.setStrokeEndCap(BL_STROKE_CAP_ROUND);
    bl_ctx.setStrokeJoin(BL_STROKE_JOIN_ROUND);
    bl_ctx.strokeLine(trace.GetStartX(), trace.GetStartY(), trace.GetEndX(), trace.GetEndY());
}

void RenderPipeline::RenderVia(BLContext &bl_ctx, const Via &via, const Board &board, const BLRect &worldViewRect)
{
    // AABB for the entire via (bounding box of all its pads)
    double max_radius = std::max(via.GetPadRadiusFrom(), via.GetPadRadiusTo());
    if (max_radius <= 0)
        max_radius = 0.01; // Ensure some extent for culling if radii are zero/negative

    BLRect via_aabb(via.GetX() - max_radius, via.GetY() - max_radius,
                    2 * max_radius, 2 * max_radius);

    if (!AreRectsIntersecting(via_aabb, worldViewRect))
    {
        return; // Cull this via
    }

    // Pad on 'from' layer
    const LayerInfo *layer_from_props = board.GetLayerById(via.GetLayerFrom());
    if (layer_from_props && layer_from_props->IsVisible() && via.GetPadRadiusFrom() > 0)
    {
        // Individual pad culling (could be redundant if via_aabb already culled, but good for precision)
        BLRect pad_from_aabb(via.GetX() - via.GetPadRadiusFrom(), via.GetY() - via.GetPadRadiusFrom(),
                             2 * via.GetPadRadiusFrom(), 2 * via.GetPadRadiusFrom());
        if (AreRectsIntersecting(pad_from_aabb, worldViewRect))
        {
            bl_ctx.setFillStyle(m_renderContext->GetBoardDataManager()->GetLayerColor(layer_from_props->GetId()));
            bl_ctx.fillCircle(via.GetX(), via.GetY(), via.GetPadRadiusFrom());
        }
    }

    // Pad on 'to' layer
    // Draw larger circle first if implementing concentric for different colors
    const LayerInfo *layer_to_props = board.GetLayerById(via.GetLayerTo());
    if (layer_to_props && layer_to_props->IsVisible() && via.GetPadRadiusTo() > 0)
    {
        BLRect pad_to_aabb(via.GetX() - via.GetPadRadiusTo(), via.GetY() - via.GetPadRadiusTo(),
                           2 * via.GetPadRadiusTo(), 2 * via.GetPadRadiusTo());
        if (AreRectsIntersecting(pad_to_aabb, worldViewRect))
        {
            // If drawing concentric, ensure this is the smaller radius or drawn on top.
            // For simple overlay, this is fine. For true concentric with different colors visible:
            // if (via.GetLayerTo() != via.GetLayerFrom() || via.GetPadRadiusTo() < via.GetPadRadiusFrom()) {
            // }
            bl_ctx.setFillStyle(m_renderContext->GetBoardDataManager()->GetLayerColor(layer_to_props->GetId()));
            bl_ctx.fillCircle(via.GetX(), via.GetY(), via.GetPadRadiusTo());
            // }
        }
    }
    // Drill hole rendering can be added here if desired.
}

void RenderPipeline::RenderArc(BLContext &bl_ctx, const Arc &arc, const BLRect &worldViewRect)
{
    // AABB for the arc (approximated by the bounding box of its circle)
    // More precise AABB would involve checking arc extents, but this is usually sufficient for culling.
    double radius = arc.GetRadius();
    double thickness_for_aabb = arc.GetThickness() > 0 ? arc.GetThickness() : 0.05;
    BLRect arc_aabb(arc.GetCX() - radius - thickness_for_aabb / 2.0,
                    arc.GetCY() - radius - thickness_for_aabb / 2.0,
                    2 * radius + thickness_for_aabb,
                    2 * radius + thickness_for_aabb);

    if (!AreRectsIntersecting(arc_aabb, worldViewRect))
    {
        return; // Cull this arc
    }

    // Color is set by RenderBoard.
    double thickness = arc.GetThickness();
    if (thickness <= 0)
    {
        thickness = 0.05; // Default thickness.
    }
    bl_ctx.setStrokeWidth(thickness);
    double start_angle_rad = arc.GetStartAngle() * (BL_M_PI / 180.0);
    double end_angle_rad = arc.GetEndAngle() * (BL_M_PI / 180.0);
    double sweep_angle_rad = end_angle_rad - start_angle_rad;

    // Ensure sweep_angle_rad is positive and represents the CCW sweep from start to end.
    // If end_angle_rad is numerically smaller than start_angle_rad (e.g. arc from 350deg to 10deg),
    // sweep_angle_rad would be negative. Adding 2*PI makes it positive for CCW direction.
    if (sweep_angle_rad < 0)
    {
        sweep_angle_rad += 2 * BL_M_PI;
    }
    // If the absolute value of sweep_angle_rad is very close to 2*PI or 0, it might indicate a full circle or no arc.
    // This logic assumes a proper arc segment is intended.

    BLPath path;
    path.arcTo(arc.GetCX(), arc.GetCY(), arc.GetRadius(), arc.GetRadius(), start_angle_rad, sweep_angle_rad);
    bl_ctx.strokePath(path);
}

void RenderPipeline::RenderComponent(BLContext &bl_ctx, const Component &component, const Board &board, const BLRect &worldViewRect, const BLRgba32 &componentBaseColor)
{
    // Calculate component's world AABB accounting for rotation
    double comp_w = component.width;
    double comp_h = component.height;
    double comp_cx = component.center_x;
    double comp_cy = component.center_y;
    double comp_rot_rad = component.rotation * (BL_M_PI / 180.0);
    double cos_r = std::cos(comp_rot_rad);
    double sin_r = std::sin(comp_rot_rad);

    // Ensure minimum dimensions for AABB calculation if width/height are zero or negative
    if (comp_w <= 0)
        comp_w = 0.1;
    if (comp_h <= 0)
        comp_h = 0.1;

    // Local corners (relative to component's local origin 0,0 before rotation/translation)
    BLPoint local_corners[4] = {
        {-comp_w / 2.0, -comp_h / 2.0}, {comp_w / 2.0, -comp_h / 2.0}, {comp_w / 2.0, comp_h / 2.0}, {-comp_w / 2.0, comp_h / 2.0}};

    BLPoint world_corners[4];
    for (int i = 0; i < 4; ++i)
    {
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
    for (int i = 1; i < 4; ++i)
    {
        min_wx = std::min(min_wx, world_corners[i].x);
        max_wx = std::max(max_wx, world_corners[i].x);
        min_wy = std::min(min_wy, world_corners[i].y);
        max_wy = std::max(max_wy, world_corners[i].y);
    }
    BLRect component_world_aabb(min_wx, min_wy, max_wx - min_wx, max_wy - min_wy);
    if (!AreRectsIntersecting(component_world_aabb, worldViewRect))
    {
        return; // Cull entire component
    }

    // Get the component's layer color
    const LayerInfo *componentLayerInfo = board.GetLayerById(component.layer);
    BLRgba32 componentColor = componentLayerInfo ? m_renderContext->GetBoardDataManager()->GetLayerColor(componentLayerInfo->GetId()) : BLRgba32(0, 0, 0, 255);

    // Draw component outline using Blend2D path
    BLPath outline;
    outline.moveTo(component.center_x - comp_w / 2.0, component.center_y - comp_h / 2.0);
    outline.lineTo(component.center_x + comp_w / 2.0, component.center_y - comp_h / 2.0);
    outline.lineTo(component.center_x + comp_w / 2.0, component.center_y + comp_h / 2.0);
    outline.lineTo(component.center_x - comp_w / 2.0, component.center_y + comp_h / 2.0);
    outline.close();

    // Set fill and stroke styles for the component
    // Use component's layer color for fill, but use componentBaseColor for outline
    bl_ctx.setFillStyle(BLRgba32(componentBaseColor.r(), componentBaseColor.g(), componentBaseColor.b(), 32)); // Semi-transparent fill based on componentBaseColor
    bl_ctx.setStrokeStyle(componentBaseColor);                                                                 // Use componentBaseColor for outline
    bl_ctx.setStrokeWidth(0.1);                                                                                // Thin outline

    // Fill and stroke the component outline
    bl_ctx.fillPath(outline);
    bl_ctx.strokePath(outline);

    // Render Component Pins
    BLRgba32 pin_net_highlight_color = componentBaseColor; // Default to component's color, override if highlighted
    BLRgba32 pin_standard_color = componentBaseColor;      // Default to component's color, override for standard pin
    int selected_net_id = -1;

    if (m_renderContext && m_renderContext->GetBoardDataManager())
    {
        pin_net_highlight_color = m_renderContext->GetBoardDataManager()->GetColor(BoardDataManager::ColorType::kNetHighlight);
        pin_standard_color = m_renderContext->GetBoardDataManager()->GetColor(BoardDataManager::ColorType::kPin);
        selected_net_id = m_renderContext->GetBoardDataManager()->GetSelectedNetId();
    }

    for (const auto &pin_ptr : component.pins)
    {
        if (pin_ptr && pin_ptr->isVisible())
        {
            bool is_pin_selected_net = (selected_net_id != -1 &&
                                        pin_ptr->getNetId() == selected_net_id);

            RenderPin(bl_ctx, *pin_ptr, &component, is_pin_selected_net ? pin_net_highlight_color : pin_standard_color);
        }
    }

    // Render Component-Specific Text Labels
    for (const auto &label_ptr : component.text_labels)
    {
        if (label_ptr && label_ptr->isVisible())
        {
            const LayerInfo *element_layer = board.GetLayerById(label_ptr->getLayerId());
            if (element_layer && element_layer->IsVisible())
            {
                RenderTextLabel(bl_ctx, *label_ptr, m_renderContext->GetBoardDataManager()->GetLayerColor(element_layer->GetId()));
            }
        }
    }
}

void RenderPipeline::RenderTextLabel(BLContext &bl_ctx, const TextLabel &textLabel, const BLRgba32 &color)
{
    if (!textLabel.isVisible() || textLabel.text_content.empty()) // Ensure isVisible() is used
    {
        return;
    }

    BLFont font;
    BLFontFace face;
    BLResult err = BL_SUCCESS;

    // Try to find cached font face
    auto it = m_fontFaceCache.find(textLabel.font_family);
    if (it != m_fontFaceCache.end())
    {
        face = it->second;
        // Optional: Check if face is valid, though it should be if cached.
        // if (!face.isValid()) { /* Handle error or remove from cache */ }
    }
    else if (!textLabel.font_family.empty())
    {
        err = face.createFromFile(textLabel.font_family.c_str());
        if (err == BL_SUCCESS)
        {
            m_fontFaceCache[textLabel.font_family] = face; // Cache successfully loaded font
        }
    }

    // If specific font failed or was not provided, or not found in cache and not specified
    if (err != BL_SUCCESS || textLabel.font_family.empty() || !face.isValid())
    {
        const char *fallbackFonts[] = {"DejaVuSans.ttf", "arial.ttf", "LiberationSans-Regular.ttf"};
        bool loadedFallback = false;
        for (const char *fontName : fallbackFonts)
        {
            // Check cache first for fallback fonts
            auto fallback_it = m_fontFaceCache.find(fontName);
            if (fallback_it != m_fontFaceCache.end())
            {
                face = fallback_it->second;
                if (face.isValid())
                {
                    loadedFallback = true;
                    break;
                }
            }
            // If not in cache, try to load and then cache it
            err = face.createFromFile(fontName);
            if (err == BL_SUCCESS)
            {
                m_fontFaceCache[fontName] = face; // Cache successfully loaded fallback font
                loadedFallback = true;
                break;
            }
        }
        if (!loadedFallback)
        {
            // std::cerr << "Failed to load any fallback font for text label: " << textLabel.text_content << std::endl;
            return;
        }
    }

    font.createFromFace(face, static_cast<float>(textLabel.font_size * textLabel.scale));

    bl_ctx.setFillStyle(color);

    if (textLabel.rotation != 0.0)
    {
        bl_ctx.save();
        bl_ctx.translate(textLabel.x, textLabel.y);
        bl_ctx.rotate(textLabel.rotation * (BL_M_PI / 180.0));
        // Using fillUtf8Text as per Blend2D documentation
        bl_ctx.fillUtf8Text(BLPoint(0, 0), font, textLabel.text_content.c_str());
        bl_ctx.restore();
    }
    else
    {
        // Using fillUtf8Text. Adjusted y for baseline approximation.
        bl_ctx.fillUtf8Text(BLPoint(textLabel.x, textLabel.y + textLabel.font_size), font, textLabel.text_content.c_str());
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
//                 path.arcTo(pin_center_x, pin_center_y - rect_height / 2.0, radius, radius, BL_M_PI, BL_M_PI); // Top cap
//                 path.lineTo(pin_center_x + radius, pin_center_y + rect_height / 2.0);
//                 path.arcTo(pin_center_x, pin_center_y + rect_height / 2.0, radius, radius, 0.0, BL_M_PI); // Bottom cap
//                 path.close();
//             } else { // Horizontal Capsule
//                 double radius = cap_cross_len / 2.0;
//                 double rect_width = cap_axis_len - cap_cross_len; // cap_axis_len is total width
//                 if (rect_width < 0) rect_width = 0;

//                 path.moveTo(pin_center_x - rect_width / 2.0, pin_center_y - radius);
//                 path.lineTo(pin_center_x + rect_width / 2.0, pin_center_y - radius);
//                 path.arcTo(pin_center_x + rect_width / 2.0, pin_center_y, radius, radius, -BL_M_PI / 2.0, BL_M_PI); // Right cap
//                 path.lineTo(pin_center_x - rect_width / 2.0, pin_center_y + radius);
//                 path.arcTo(pin_center_x - rect_width / 2.0, pin_center_y, radius, radius, BL_M_PI / 2.0, BL_M_PI); // Left cap
//                 path.close();
//             }
//             bl_ctx.fillPath(path);
//         } }, pin.pad_shape);
// }
// Render a pin using Blend2D, automatically handling all shape types
void RenderPipeline::RenderPin(BLContext &ctx, const Pin &pin, const Component *parentComponent, const BLRgba32 &highlightColor)
{
    auto [world_center, world_rotation] = Pin::GetPinWorldTransform(pin, parentComponent);
    auto [width, height] = pin.getDimensions();
    double x_coord = pin.x_coord;
    double y_coord = pin.y_coord;
    PinOrientation orientation = pin.orientation;

    // Move to pin center and apply rotation if needed
    ctx.setFillStyle(highlightColor);
    // Render based on shape type
    std::visit([&ctx, width, height, x_coord, y_coord, orientation](const auto &shape)
               {
            using T = std::decay_t<decltype(shape)>;
            
            if constexpr (std::is_same_v<T, CirclePad>) {
                BLCircle circle(x_coord, y_coord, shape.radius);
                ctx.fillCircle(circle);
            }
            else if constexpr (std::is_same_v<T, RectanglePad>) {
                BLRect rect(x_coord - width/2.0, y_coord - height/2.0, width, height);
                ctx.fillRect(rect);
            }
            else if constexpr (std::is_same_v<T, CapsulePad>) {
                    renderCapsule(ctx, shape.width, shape.height, x_coord, y_coord);
                } }, pin.pad_shape);
}

static void renderCapsule(BLContext &ctx, double width, double height, double x_coord, double y_coord)
{
    double radius = std::min(width, height) / 2.0; // Radius is half the smaller dimension

    // Create a rounded rectangle centered at x_coord, y_coord
    BLRoundRect capsule(x_coord - width / 2.0, y_coord - height / 2.0, width, height, radius);
    ctx.fillRoundRect(capsule);
}

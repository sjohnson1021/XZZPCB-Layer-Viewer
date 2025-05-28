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
        RenderBoard(bl_ctx, *board, camera, viewport);
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
    const Viewport &viewport) // Removed grid parameter
{
    // std::cout << "RenderBoard (Corrected Logic): Board has " << board.layers.size() << " layers defined." << std::endl;

    // Get the visible world rectangle for culling

    bl_ctx.save();

    bl_ctx.translate(viewport.GetWidth() / 2.0, viewport.GetHeight() / 2.0);
    bl_ctx.scale(camera.GetZoom());
    // Use cached rotation values from camera
    // float rotationRadians = camera.GetRotation() * (static_cast<float>(BL_M_PI) / 180.0f);
    // This line is not strictly needed if camera.GetRotation gives the angle for GetCachedSin/Cos
    // but RenderPipeline directly applies rotation here. It can also use the cached sin/cos.
    // For direct rotation, Blend2D expects radians. If GetRotation is degrees:
    // float rad = camera.GetRotation() * (BL_M_PI / 180.0f);
    // bl_ctx.rotate(rad);
    // OR, if we want to be consistent and use the cached values (which might be slightly out of sync if SetRotation was called but not yet reflected in a full matrix)
    // For Blend2D's rotate, it takes the angle. The cached sin/cos are for constructing matrices usually.
    // Keeping it simple for now with direct angle conversion.
    // Flipping rotation direction to match the grid, assuming grid's current rotation is the desired one.
    bl_ctx.rotate(-camera.GetRotation() * (static_cast<float>(BL_M_PI) / 180.0f));

    // Apply camera translation: Use positive Y component of camera position
    // to make board's Y movement consistent with grid's Y movement relative to camera world Y changes.
    // With a Y-Down world system for the camera, and Blend2D being Y-Down:
    // If camera.GetPosition().y increases (camera moves "down" in world), the world should appear to move "up".
    // Blend2D translation by a negative Y moves content up.
    // So, we translate by -camera.GetPosition().y.
    bl_ctx.translate(-camera.GetPosition().x, -camera.GetPosition().y);
    BLRect worldViewRect = GetVisibleWorldBounds(camera, viewport);

    // Render Traces
    for (const auto &trace : board.GetTraces())
    {
        const LayerInfo *layer_info = board.GetLayerById(trace.GetLayer());
        if (layer_info && layer_info->IsVisible())
        {
            bl_ctx.setStrokeStyle(layer_info->GetColor());
            RenderTrace(bl_ctx, trace, worldViewRect);
        }
    }

    // Render Arcs
    for (const auto &arc : board.GetArcs())
    {
        const LayerInfo *layer_info = board.GetLayerById(arc.GetLayer());
        if (layer_info && layer_info->IsVisible())
        {
            bl_ctx.setStrokeStyle(layer_info->GetColor());
            RenderArc(bl_ctx, arc, worldViewRect);
        }
    }

    // Render Vias
    for (const auto &via : board.GetVias())
    {
        // Via visibility is often tied to layers, but pads might span multiple visible layers.
        // Culling individual pads within RenderVia.
        RenderVia(bl_ctx, via, board, worldViewRect);
    }

    // Render Components
    for (const auto &component : board.GetComponents())
    {
        // RenderComponent will handle its own culling and layer visibility internally
        RenderComponent(bl_ctx, component, board, worldViewRect);
    }

    // Render Standalone Text Labels
    for (const auto &text_label : board.GetTextLabels())
    {
        if (!text_label.is_visible)
            continue;

        // AABB for text label (approximate)
        // This is a rough approximation. For accurate AABB, we'd need font metrics.
        // Assuming font_size is in world units for now.
        // The text is drawn with its origin at (textLabel.x, textLabel.y)
        // Rotation makes AABB more complex. For simplicity, we'll use an unrotated AABB
        // around the origin point, scaled by font size.
        // A more accurate AABB would consider the text block's actual rendered extent.
        double approx_text_width = text_label.text_content.length() * text_label.font_size * 0.6; // Rough guess
        double approx_text_height = text_label.font_size;
        BLRect text_aabb(
            text_label.x - approx_text_width / 2.0, // Assuming centered for culling, adjust if origin is top-left
            text_label.y - approx_text_height / 2.0,
            approx_text_width,
            approx_text_height);
        // If rotation is significant, this AABB needs to be expanded or transformed.
        // For simplicity, we'll use this, but it might incorrectly cull rotated text.

        if (!AreRectsIntersecting(text_aabb, worldViewRect))
        {
            continue;
        }

        const LayerInfo *layer_info = board.GetLayerById(text_label.layer);
        if (layer_info && layer_info->IsVisible())
        {
            RenderTextLabel(bl_ctx, text_label, layer_info->GetColor());
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

    static bool first_trace_logged_after_refactor = false;
    if (!first_trace_logged_after_refactor)
    {
        BLMatrix2D ctm;
        ctm = bl_ctx.userTransform();
        BLPoint world_start(trace.GetStartX(), trace.GetStartY());
        BLPoint world_end(trace.GetEndX(), trace.GetEndY());
        BLPoint screen_start = ctm.mapPoint(world_start);
        BLPoint screen_end = ctm.mapPoint(world_end);

        std::cout << "[RenderTrace First Call Log] Layer=" << trace.GetLayer()
                  << " WorldS: (" << world_start.x << "," << world_start.y << ")"
                  << " WorldE: (" << world_end.x << "," << world_end.y << ")"
                  << " ScreenS: (" << screen_start.x << "," << screen_start.y << ")"
                  << " ScreenE: (" << screen_end.x << "," << screen_end.y << ")"
                  << " CTM: [" << ctm.m00 << "," << ctm.m01 << "; "
                  << ctm.m10 << "," << ctm.m11 << "; "
                  << ctm.m20 << "," << ctm.m21 << "]" << std::endl;
        first_trace_logged_after_refactor = true;
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
            bl_ctx.setFillStyle(layer_from_props->GetColor());
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
            bl_ctx.setFillStyle(layer_to_props->GetColor());
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

void RenderPipeline::RenderComponent(BLContext &bl_ctx, const Component &component, const Board &board, const BLRect &worldViewRect)
{
    // Calculate component's world AABB accounting for rotation, without using a BLMatrix2D object or TransformAABB for this step.
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

    // Debug print for component "N1"
    if (component.reference_designator == "N1")
    {
        printf("RenderComponent (Manual AABB): %s\n", component.reference_designator.c_str());
        printf("  Comp Data: cx=%.2f, cy=%.2f, w=%.2f, h=%.2f, rot=%.2f\n",
               component.center_x, component.center_y, component.width, component.height, component.rotation);
        printf("  Calculated World AABB: x=%.2f, y=%.2f, w=%.2f, h=%.2f\n",
               component_world_aabb.x, component_world_aabb.y, component_world_aabb.w, component_world_aabb.h);
        printf("  WorldViewRect: x=%.2f, y=%.2f, w=%.2f, h=%.2f\n",
               worldViewRect.x, worldViewRect.y, worldViewRect.w, worldViewRect.h);
    }

    if (!AreRectsIntersecting(component_world_aabb, worldViewRect))
    {
        if (component.reference_designator == "N1")
        {
            printf("Culled component (Manual AABB): %s.\n", component.reference_designator.c_str());
        }
        return; // Cull entire component
    }

    // The BLMatrix2D componentTransform and TransformAABB calls for culling were here and are intentionally removed/commented out.
    // Culling for individual sub-elements (segments, labels, pins) also remains commented out from the previous step.

    // bl_ctx.save();
    // bl_ctx.translate(component.center_x, component.center_y);
    // bl_ctx.rotate(component.rotation * (BL_M_PI / 180.0));

    // Render Graphical Elements
    for (const auto &segment : component.graphical_elements)
    {
        const LayerInfo *element_layer = board.GetLayerById(segment.layer);
        if (element_layer && element_layer->IsVisible())
        {
            bl_ctx.setStrokeStyle(element_layer->GetColor());
            double thickness = segment.thickness > 0 ? segment.thickness : 0.1; // Default thickness
            bl_ctx.setStrokeWidth(thickness);
            bl_ctx.strokeLine(segment.start.x, segment.start.y, segment.end.x, segment.end.y);
        }
    }

    // Render Component-Specific Text Labels
    for (const auto &label : component.text_labels)
    {
        if (!label.is_visible)
            continue;

        const LayerInfo *element_layer = board.GetLayerById(label.layer);
        if (element_layer && element_layer->IsVisible())
        {
            RenderTextLabel(bl_ctx, label, element_layer->GetColor());
        }
    }

    // Render Pins
    for (const auto &pin : component.pins)
    {
        RenderPin(bl_ctx, pin, component, board);
    }

    // bl_ctx.restore();
}

void RenderPipeline::RenderTextLabel(BLContext &bl_ctx, const TextLabel &textLabel, const BLRgba32 &color)
{
    if (!textLabel.is_visible || textLabel.text_content.empty())
    {
        return;
    }

    // Font setup
    BLFont font;
    BLFontFace face;
    BLResult err = BL_SUCCESS; // Initialize err

    if (!textLabel.font_family.empty())
    {
        std::cerr << "Attempting to load font family: " << textLabel.font_family << std::endl;
        err = face.createFromFile(textLabel.font_family.c_str());
        std::cerr << "Result for '" << textLabel.font_family << "': " << err << std::endl;
    }

    if (err != BL_SUCCESS)
    { // If specific font failed or was not provided, try fallbacks
        const char *fallbackFonts[] = {"DejaVuSans.ttf", "arial.ttf", "LiberationSans-Regular.ttf"};
        bool loadedFallback = false;
        for (const char *fontName : fallbackFonts)
        {
            std::cerr << "Attempting to load fallback font: " << fontName << std::endl;
            err = face.createFromFile(fontName);
            std::cerr << "Result for '" << fontName << "': " << err << std::endl;
            if (err == BL_SUCCESS)
            {
                std::cerr << "Successfully loaded fallback font: " << fontName << std::endl;
                loadedFallback = true;
                break;
            }
        }
        if (!loadedFallback)
        {
            std::cerr << "Failed to load any fallback font for text label: " << textLabel.text_content << std::endl;
            return; // If no font could be loaded, cannot render text.
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

// ***** BEGIN ADDED CODE *****
void RenderPipeline::RenderPin(BLContext &bl_ctx, const Pin &pin, const Component &component, const Board &board)
{
    const LayerInfo *layer_info = board.GetLayerById(pin.layer); // Assuming pin has a 'layer' member
    if (!layer_info || !layer_info->IsVisible())
    {
        return;
    }

    bl_ctx.setFillStyle(layer_info->GetColor());
    // DEBUG: If pin.debug_color is set, use it instead of layer_info->GetColor
    // if (pin.debug_color != BLRgba32(0, 0, 0, 0))
    // {
    //     bl_ctx.setFillStyle(pin.debug_color);
    //     printf("[RenderPin]: Pin %s: Using debug color %d\n", pin.pin_name.c_str(), pin.debug_color);
    // }
    // NOTE: The component's main transformation (center_x, center_y, rotation)
    // is ALREADY APPLIED to bl_ctx by the calling RenderComponent method.
    // So, pin.x_coord, pin.y_coord, and shape.x_offset/y_offset are treated as local
    // to the component's origin.

    std::visit([&](auto &&shape) // Note: auto&& shape is important here for perfect forwarding
               {
        using T = std::decay_t<decltype(shape)>;
        // Calculate effective pin center in component's local space
        double local_pin_center_x = pin.x_coord + shape.x_offset;
        double local_pin_center_y = pin.y_coord + shape.y_offset;

        if constexpr (std::is_same_v<T, CirclePad>) {
            bl_ctx.fillCircle(local_pin_center_x, local_pin_center_y, shape.radius);
        } else if constexpr (std::is_same_v<T, RectanglePad>) {
            double width = 0;
            double height = 0;

            if (pin.orientation == PinOrientation::Vertical) {
                height = pin.initial_height;
                width = pin.initial_width;
            } else if (pin.orientation == PinOrientation::Horizontal) {
                height = pin.initial_width;
                width = pin.initial_height;
            } else {
                height = pin.initial_height;
                width = pin.initial_width;
            }

            // if pin edge interior. and component is taller than wide, then pin should be taller than wide.
            if (pin.local_edge == LocalEdge::INTERIOR && component.height > component.width) {
                if (pin.initial_width > pin.initial_height) {
                    height = pin.initial_width;
                    width = pin.initial_height;
                } else {
                    width = pin.initial_height;
                    height = pin.initial_width;
                }
            } else if (pin.local_edge == LocalEdge::INTERIOR && component.height < component.width) {
                if (pin.initial_width > pin.initial_height) {
                    width = pin.initial_width;
                    height = pin.initial_height;
                } else {
                    width = pin.initial_height;
                    height = pin.initial_width;
                }
            }
            // Draw centered rectangle at local_pin_center_x, local_pin_center_y
            bl_ctx.fillRect(BLRect(local_pin_center_x - width / 2.0, local_pin_center_y - height / 2.0, width, height));

        } else if constexpr (std::is_same_v<T, CapsulePad>) {
            double total_width = shape.width;
            double total_height = shape.height;
            // Effective pad center is local_pin_center_x, local_pin_center_y

            bool is_effectively_vertical = false;
            if (pin.orientation == PinOrientation::Vertical) {
                if (shape.width > shape.height) { 
                    total_width = shape.height;
                    total_height = shape.width;
                }
                is_effectively_vertical = true;
            } else if (pin.orientation == PinOrientation::Horizontal) {
                 if (shape.height > shape.width) { 
                    total_width = shape.height;
                    total_height = shape.width;
                }
                is_effectively_vertical = false;
            } else { // PinOrientation::Natural
                is_effectively_vertical = (shape.height > shape.width);
            }

            BLPath path;
            if (is_effectively_vertical) { 
                double radius = total_width / 2.0;
                double rect_height = total_height - total_width;
                if (rect_height < 0) rect_height = 0; 

                path.moveTo(local_pin_center_x - radius, local_pin_center_y - rect_height / 2.0);
                path.arcTo(local_pin_center_x, local_pin_center_y - rect_height / 2.0, radius, radius, BL_M_PI, BL_M_PI);
                path.lineTo(local_pin_center_x + radius, local_pin_center_y + rect_height / 2.0);
                path.arcTo(local_pin_center_x, local_pin_center_y + rect_height / 2.0, radius, radius, 0.0, BL_M_PI);
                path.close();
            } else { 
                double radius = total_height / 2.0;
                double rect_width = total_width - total_height;
                if (rect_width < 0) rect_width = 0;

                path.moveTo(local_pin_center_x - rect_width / 2.0, local_pin_center_y - radius);
                path.lineTo(local_pin_center_x + rect_width / 2.0, local_pin_center_y - radius);
                path.arcTo(local_pin_center_x + rect_width / 2.0, local_pin_center_y, radius, radius, -BL_M_PI / 2.0, BL_M_PI);
                path.lineTo(local_pin_center_x - rect_width / 2.0, local_pin_center_y + radius);
                path.arcTo(local_pin_center_x - rect_width / 2.0, local_pin_center_y, radius, radius, BL_M_PI / 2.0, BL_M_PI);
                path.close();
            }
            bl_ctx.fillPath(path);
        } }, pin.pad_shape);
}
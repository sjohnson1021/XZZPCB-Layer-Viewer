#include "render/RenderPipeline.hpp"
#include "render/RenderContext.hpp" // For access to RenderContext if needed
#include "pcb/Board.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "view/Grid.hpp" // For Grid class definition for RenderGrid
#include "pcb/elements/Trace.hpp" // Added for Trace element
#include "pcb/elements/Via.hpp"   // Added for Via element
#include "pcb/elements/Arc.hpp"   // Added for Arc element
#include "pcb/elements/Component.hpp" // Added for Component
#include "pcb/elements/TextLabel.hpp" // Added for TextLabel
#include "pcb/elements/Pin.hpp" // For Pin and PadShape when rendering pins
#include <blend2d.h>
#include <iostream> // For logging
#include <iomanip>  // For std::hex/std::dec output manipulator

// For logging or error reporting:
// #include <iostream>
// #include <SDL3/SDL_log.h>

RenderPipeline::RenderPipeline() : m_renderContext(nullptr), m_initialized(false) {
    // std::cout << "RenderPipeline created." << std::endl;
}

RenderPipeline::~RenderPipeline() {
    // Ensure Shutdown is called if not already.
    if (m_initialized) {
        // std::cerr << "RenderPipeline destroyed without calling Shutdown() first!" << std::endl;
        Shutdown();
    }
    // std::cout << "RenderPipeline destroyed." << std::endl;
}

bool RenderPipeline::Initialize(RenderContext& context) {
    m_renderContext = &context; // Store pointer to the context
    // Initialize any pipeline-specific resources if needed.
    // For now, it mainly relies on the context passed during Execute.
    std::cout << "RenderPipeline initialized." << std::endl;
    m_initialized = true;
    return true;
}

void RenderPipeline::Shutdown() {
    // Release any pipeline-specific resources.
    m_renderContext = nullptr;
    // Release all pipeline resources, clear stages.
    // for (auto& stage : m_stages) {
    //     if (stage) stage->Shutdown();
    // }
    // m_stages.clear();

    std::cout << "RenderPipeline shutdown." << std::endl;
    m_initialized = false;
}

void RenderPipeline::BeginScene(BLContext& bl_ctx) {
    if (!m_initialized) return;
    // This method is called by PcbRenderer.
    // It can be used to set up any per-scene state on the bl_ctx if necessary.
    // For now, PcbRenderer->BeginFrame() clears the target, which is sufficient.
    // bl_ctx.save(); // Example: save context state if pipeline makes many changes
    // std::cout << "RenderPipeline: Beginning scene." << std::endl;
}

void RenderPipeline::EndScene() {
    if (!m_initialized) return;
    // Called by PcbRenderer after Execute.
    // Can be used to restore context state if saved in BeginScene.
    // if (m_renderContext && m_renderContext->GetBlend2DContext().isActive()) { // Check if context is valid
    //     m_renderContext->GetBlend2DContext().restore(); // Example: restore context state
    // }
    // std::cout << "RenderPipeline: Ending scene." << std::endl;
}

void RenderPipeline::Execute(
    BLContext& bl_ctx, 
    const Board* board,
    const Camera& camera, 
    const Viewport& viewport,
    const Grid& grid) 
{
    if (!m_initialized) {
        std::cerr << "RenderPipeline::Execute Error: Not initialized." << std::endl;
        return;
    }

    // Render the grid first, so it's behind board elements.
    // The Grid::Render method will apply its own transformations using the camera and viewport.
    RenderGrid(bl_ctx, camera, viewport, grid); 

    if (board) { // Only render board if it's not null
        RenderBoard(bl_ctx, *board, camera, viewport); // Removed grid parameter
    }
}

void RenderPipeline::RenderBoard(
    BLContext& bl_ctx,
    const Board& board,
    const Camera& camera,
    const Viewport& viewport) // Removed grid parameter
{
    // std::cout << "RenderBoard (Corrected Logic): Board has " << board.layers.size() << " layers defined." << std::endl; 

    bl_ctx.save(); 
    
    bl_ctx.translate(viewport.GetWidth() / 2.0, viewport.GetHeight() / 2.0);
    bl_ctx.scale(camera.GetZoom());
    bl_ctx.rotate(camera.GetRotation() * (BL_M_PI / 180.0));
    // Apply camera translation: Use positive Y component of camera position
    // to make board's Y movement consistent with grid's Y movement relative to camera world Y changes.
    bl_ctx.translate(-camera.GetPosition().x, camera.GetPosition().y);

    // Render Traces
    for (const auto& trace : board.GetTraces()) {
        const LayerInfo* layer_info = board.GetLayerById(trace.GetLayer());
        if (layer_info && layer_info->IsVisible()) {
            bl_ctx.setStrokeStyle(layer_info->GetColor());
            RenderTrace(bl_ctx, trace);
        }
    }

    // Render Arcs
    for (const auto& arc : board.GetArcs()) {
        const LayerInfo* layer_info = board.GetLayerById(arc.GetLayer());
        if (layer_info && layer_info->IsVisible()) {
            bl_ctx.setStrokeStyle(layer_info->GetColor());
            RenderArc(bl_ctx, arc);
        }
    }
    
    // Render Vias
    for (const auto& via : board.GetVias()) {
        RenderVia(bl_ctx, via, board);
    }

    // Render Components
    for (const auto& component : board.GetComponents()) {
        RenderComponent(bl_ctx, component, board);
    }
    
    // Render Standalone Text Labels
    for (const auto& text_label : board.GetTextLabels()) {
        if (!text_label.is_visible) continue;
        const LayerInfo* layer_info = board.GetLayerById(text_label.layer);
        if (layer_info && layer_info->IsVisible()) {
            RenderTextLabel(bl_ctx, text_label, layer_info->GetColor());
        }
    }
    
    bl_ctx.restore(); 
}

void RenderPipeline::RenderGrid(
    BLContext& bl_ctx, 
    const Camera& camera, 
    const Viewport& viewport,
    const Grid& grid) 
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

void RenderPipeline::RenderTrace(BLContext& bl_ctx, const Trace& trace) {
    // Color is set by RenderBoard.
    double width = trace.GetWidth();
    if (width <= 0) { 
        width = 0.05; // Default width. TODO: Configurable or dynamic based on zoom.
    }

    static bool first_trace_logged_after_refactor = false;
    if (!first_trace_logged_after_refactor) {
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

void RenderPipeline::RenderVia(BLContext& bl_ctx, const Via& via, const Board& board) {
    // Pad on 'from' layer
    const LayerInfo* layer_from_props = board.GetLayerById(via.GetLayerFrom());
    if (layer_from_props && layer_from_props->IsVisible() && via.GetPadRadiusFrom() > 0) {
        bl_ctx.setFillStyle(layer_from_props->GetColor());
        bl_ctx.fillCircle(via.GetX(), via.GetY(), via.GetPadRadiusFrom());
    }

    // Pad on 'to' layer
    // Draw larger circle first if implementing concentric for different colors
    const LayerInfo* layer_to_props = board.GetLayerById(via.GetLayerTo());
    if (layer_to_props && layer_to_props->IsVisible() && via.GetPadRadiusTo() > 0) {
        // If drawing concentric, ensure this is the smaller radius or drawn on top.
        // For simple overlay, this is fine. For true concentric with different colors visible:
        // if (via.GetLayerTo() != via.GetLayerFrom() || via.GetPadRadiusTo() < via.GetPadRadiusFrom()) {
             bl_ctx.setFillStyle(layer_to_props->GetColor());
             bl_ctx.fillCircle(via.GetX(), via.GetY(), via.GetPadRadiusTo());
        // }
    }
    // Drill hole rendering can be added here if desired.
}

void RenderPipeline::RenderArc(BLContext& bl_ctx, const Arc& arc) {
    // Color is set by RenderBoard.
    double thickness = arc.GetThickness();
    if (thickness <= 0) {
        thickness = 0.05; // Default thickness.
    }
    bl_ctx.setStrokeWidth(thickness);
    bl_ctx.setStrokeStartCap(BL_STROKE_CAP_ROUND);
    bl_ctx.setStrokeEndCap(BL_STROKE_CAP_ROUND);
    bl_ctx.setStrokeJoin(BL_STROKE_JOIN_ROUND);

    double start_angle_rad = arc.GetStartAngle() * (BL_M_PI / 180.0);
    double end_angle_rad = arc.GetEndAngle() * (BL_M_PI / 180.0);
    double sweep_angle_rad = end_angle_rad - start_angle_rad;

    // Ensure sweep_angle_rad is positive and represents the CCW sweep from start to end.
    // If end_angle_rad is numerically smaller than start_angle_rad (e.g. arc from 350deg to 10deg),
    // sweep_angle_rad would be negative. Adding 2*PI makes it positive for CCW direction.
    if (sweep_angle_rad < 0) {
        sweep_angle_rad += 2 * BL_M_PI;
    }
    // If the absolute value of sweep_angle_rad is very close to 2*PI or 0, it might indicate a full circle or no arc.
    // This logic assumes a proper arc segment is intended.

    BLPath path;
    path.arcTo(arc.GetCX(), arc.GetCY(), arc.GetRadius(), arc.GetRadius(), start_angle_rad, sweep_angle_rad);
    bl_ctx.strokePath(path);
}

void RenderPipeline::RenderComponent(BLContext& bl_ctx, const Component& component, const Board& board) {
    // Render Graphical Elements
    for (const auto& segment : component.graphical_elements) {
        const LayerInfo* element_layer = board.GetLayerById(segment.layer);
        if (element_layer && element_layer->IsVisible()) {
            bl_ctx.setStrokeStyle(element_layer->GetColor());
            double thickness = segment.thickness;
            if (thickness <= 0) {
                thickness = 0.1; 
            }
            bl_ctx.setStrokeWidth(thickness);
            bl_ctx.strokeLine(segment.start.x, segment.start.y, segment.end.x, segment.end.y);
        }
    }

    // Render Component-Specific Text Labels
    for (const auto& label : component.text_labels) {
        if (!label.is_visible) continue;
        const LayerInfo* element_layer = board.GetLayerById(label.layer);
        if (element_layer && element_layer->IsVisible()) {
            RenderTextLabel(bl_ctx, label, element_layer->GetColor());
        }
    }
    // Pin rendering TODO: Iterate component.pins, get pin's layer, look up LayerInfo, draw pad shape with color.
}

void RenderPipeline::RenderTextLabel(BLContext& bl_ctx, const TextLabel& textLabel, const BLRgba32& color) {
    if (!textLabel.is_visible || textLabel.text_content.empty()) {
        return;
    }

    // Font setup
    BLFont font;
    BLFontFace face;
    BLResult err = BL_SUCCESS; // Initialize err

    if (!textLabel.font_family.empty()) {
        std::cerr << "Attempting to load font family: " << textLabel.font_family << std::endl;
        err = face.createFromFile(textLabel.font_family.c_str());
        std::cerr << "Result for '" << textLabel.font_family << "': " << err << std::endl;
    }
    
    if (err != BL_SUCCESS) { // If specific font failed or was not provided, try fallbacks
        const char* fallbackFonts[] = {"DejaVuSans.ttf", "arial.ttf", "LiberationSans-Regular.ttf"};
        bool loadedFallback = false;
        for (const char* fontName : fallbackFonts) {
            std::cerr << "Attempting to load fallback font: " << fontName << std::endl;
            err = face.createFromFile(fontName);
            std::cerr << "Result for '" << fontName << "': " << err << std::endl;
            if (err == BL_SUCCESS) {
                std::cerr << "Successfully loaded fallback font: " << fontName << std::endl;
                loadedFallback = true;
                break;
            }
        }
        if (!loadedFallback) {
             std::cerr << "Failed to load any fallback font for text label: " << textLabel.text_content << std::endl;
             return; // If no font could be loaded, cannot render text.
        }
    }

    font.createFromFace(face, static_cast<float>(textLabel.font_size * textLabel.scale));

    bl_ctx.setFillStyle(color);

    if (textLabel.rotation != 0.0) {
        bl_ctx.save();
        bl_ctx.translate(textLabel.x, textLabel.y);
        bl_ctx.rotate(textLabel.rotation * (BL_M_PI / 180.0));
        // Using fillUtf8Text as per Blend2D documentation
        bl_ctx.fillUtf8Text(BLPoint(0,0), font, textLabel.text_content.c_str()); 
        bl_ctx.restore();
    } else {
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
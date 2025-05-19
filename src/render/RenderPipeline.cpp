#include "render/RenderPipeline.hpp"
#include "render/RenderContext.hpp" // For access to RenderContext if needed
#include "pcb/Board.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "view/Grid.hpp" // For Grid class definition for RenderGrid
#include <blend2d.h>
#include <iostream> // For logging

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

    // Core rendering logic happens here
    // 1. Render the Grid (always, if grid object is valid and visible)
    RenderGrid(bl_ctx, camera, viewport, grid);

    // 2. Render the PCB Board content (only if board is not null)
    if (board) {
        RenderBoard(bl_ctx, *board, camera, viewport);
    }

    // Potentially other rendering passes could be added here.
}

void RenderPipeline::RenderBoard(
    BLContext& bl_ctx,
    const Board& board,
    const Camera& camera,
    const Viewport& viewport)
{
    // Use the Blend2D transform matrix for camera transformation
    // BLMatrix2D transform; // No longer needed
    // transform.reset(); // No longer needed
    
    bl_ctx.save(); // Save current state
    
    // Set up the world transform for correct scaling and panning
    // These operations modify the context's current transformation matrix
    bl_ctx.translate(viewport.GetWidth() / 2.0, viewport.GetHeight() / 2.0);
    bl_ctx.scale(camera.GetZoom());
    bl_ctx.rotate(camera.GetRotation() * (BL_M_PI / 180.0)); // Convert degrees to radians
    bl_ctx.translate(-camera.GetPosition().x, -camera.GetPosition().y);
    
    // Batch similar operations (placeholders for now)
    // RenderTraces(bl_ctx, board);
    // RenderPads(bl_ctx, board);
    // RenderComponents(bl_ctx, board);
    
    // Placeholder: Example of drawing something simple to verify transform
    // bl_ctx.setStrokeStyle(BLRgba32(0xFFFFFFFF)); // White color
    // bl_ctx.setStrokeWidth(1.0 / camera.GetZoom()); // Keep stroke width consistent regardless of zoom
    // bl_ctx.strokeRect(BLRect(0, 0, 100, 100)); // Draw a 100x100 rect at board origin

    // TODO: Iterate through board layers and render their content
    // For now, we'll add a simple placeholder to indicate board rendering area
    // This will be replaced with actual layer and feature rendering calls

    // Example: if board has a method to get all renderable items
    // for (const auto& item : board.GetAllItems()) {
    //     item->Render(bl_ctx, camera); // Assuming items have a Render method
    // }

    // Restore matrix and other states if changed
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
#pragma once

#include <vector>
#include <memory>
#include <blend2d.h> // Include for BLContext

// Forward declarations
class RenderContext; // The Blend2D-focused RenderContext
class Board;
class Camera;
class Viewport;
class Grid; // Forward declare Grid

class RenderPipeline {
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
    void BeginScene(BLContext& bl_ctx); // Use BLContext directly
    void EndScene(); // Might be a no-op for current plans

    // Main execution method to draw PCB elements and grid
    void Execute(
        BLContext& bl_ctx, 
        const Board* board, // Changed from const Board& to allow nullptr
        const Camera& camera, 
        const Viewport& viewport,
        const Grid& grid // Pass Grid by const reference
    );
    const double& BL_M_PI = 3.14159265358979323846;
    // Process a single renderable item (or this could be a list)
    // void Process(RenderContext& context, IRenderable* renderable);
    // Or, the pipeline might manage a list of renderables itself
    // void AddRenderable(IRenderable* renderable);
    // void RemoveRenderable(IRenderable* renderable);
    // void Execute(RenderContext& context); // Executes all stages on all renderables

    // If we have distinct stages:
    // void AddStage(std::unique_ptr<RenderStage> stage);

private:
    // std::vector<std::unique_ptr<RenderStage>> m_stages;
    // Or a more direct approach if stages are fixed:
    // void GeometryPass(RenderContext& context);
    // void LightingPass(RenderContext& context);
    // void PostProcessingPass(RenderContext& context);
    // void UIPass(RenderContext& context);

    // Helper methods for drawing specific parts, called from Execute
    void RenderBoard(
        BLContext& bl_ctx, 
        const Board& board, 
        const Camera& camera, 
        const Viewport& viewport
    );
    
    // The roadmap states: "The PcbRenderer / RenderManager will call this method [Grid::Render()] during its Render pass..."
    // However, the RenderPipeline is also tasked with drawing the grid.
    // To keep PcbRenderer simple, RenderPipeline will be responsible for calling Grid::Render().
    void RenderGrid(
        BLContext& bl_ctx, 
        const Camera& camera, 
        const Viewport& viewport,
        const Grid& grid // Pass Grid by const reference
    );

    RenderContext* m_renderContext = nullptr; // Store a pointer to the context if needed by multiple methods
    bool m_initialized = false;
    // Add any other members needed for managing rendering state or resources for the pipeline
}; 
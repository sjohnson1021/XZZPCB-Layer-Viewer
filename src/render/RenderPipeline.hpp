#pragma once

#include <vector>
#include <memory>
#include <string>              // Added for std::string
#include <map>                 // Added for std::map
#include <blend2d.h>           // Include for BLContext
#include "utils/Constants.hpp" // For BL_M_PI
#include <unordered_map>
#include "core/BoardDataManager.hpp"

// Forward declarations
class RenderContext; // The Blend2D-focused RenderContext
class Board;         // Board is now central for layer info lookup
class Camera;
class Viewport;
class Grid;      // Forward declare Grid
class Trace;     // Forward declare Trace
class Via;       // Forward declare Via
class Arc;       // Forward declare Arc
class Component; // Forward declare Component
class TextLabel; // Forward declare TextLabel
class Pin;       // Forward declare Pin
// class Layer; // No longer forward-declaring the old Layer class assumption

class RenderPipeline
{
public:
    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline &) = delete;
    RenderPipeline &operator=(const RenderPipeline &) = delete;
    RenderPipeline(RenderPipeline &&) = delete;
    RenderPipeline &operator=(RenderPipeline &&) = delete;

    // Initialize with the Blend2D-focused RenderContext
    bool Initialize(RenderContext &context);
    void Shutdown();

    // Scene-level operations for the pipeline using a BLContext
    void BeginScene(BLContext &bl_ctx); // Use BLContext directly
    void EndScene();                    // Might be a no-op for current plans

    // Main execution method to draw PCB elements and grid
    void Execute(
        BLContext &bl_ctx,
        const Board *board, // Changed from const Board& to allow nullptr
        const Camera &camera,
        const Viewport &viewport,
        const Grid &grid, // Pass Grid by const reference
        bool render_grid, // New: flag to control grid rendering
        bool render_board // New: flag to control board rendering
    );

    BLMatrix2D ViewMatrix(BLContext &bl_ctx, const Camera &camera, const Viewport &viewport);
    // Process a single renderable item (or this could be a list)
    // void Process(RenderContext& context, IRenderable* renderable);
    // Or, the pipeline might manage a list of renderables itself
    // void AddRenderable(IRenderable* renderable);
    // void RemoveRenderable(IRenderable* renderable);
    // void Execute(RenderContext& context); // Executes all stages on all renderables

    // If we have distinct stages:
    // void AddStage(std::unique_ptr<RenderStage> stage);

    // To keep PcbRenderer simple, RenderPipeline will be responsible for calling Grid::Render().
    void RenderGrid(
        BLContext &bl_ctx,
        const Camera &camera,
        const Viewport &viewport,
        const Grid &grid // Pass Grid by const reference
    );

    // Helper methods for rendering specific PCB elements
    void RenderTrace(BLContext &bl_ctx, const Trace &trace, const BLRect &world_view_rect, BLStrokeCap start_cap, BLStrokeCap end_cap);
    void RenderVia(BLContext &bl_ctx, const Via &via, const Board &board, const BLRect &world_view_rect, const BLRgba32 &color_from, const BLRgba32 &color_to);
    void RenderArc(BLContext &bl_ctx, const Arc &arc, const BLRect &world_view_rect);
    void RenderComponent(
        BLContext &bl_ctx,
        const Component &component,
        const Board &board,
        const BLRect &world_view_rect,
        const BLRgba32 &component_base_color,
        const std::unordered_map<BoardDataManager::ColorType, BLRgba32> &theme_color_cache,
        int selected_net_id);
    void RenderTextLabel(BLContext &bl_ctx, const TextLabel &text_label, const BLRgba32 &color);
    // TODO: Consider passing layer_properties_map to RenderTextLabel if it needs more than just color
    // void RenderPin(BLContext &bl_ctx, const Pin &pin, const Component &component, const Board &board, const BLRgba32 &highlightColor);
    void RenderPin(BLContext &ctx, const Pin &pin, const Component *parent_component, const BLRgba32 &highlight_color);

private:
    // Helper to get a cached font face or load it if not found
    BLResult GetOrCreateFontFace(const std::string &font_family_path, BLFontFace &out_face);

    // Helper function to get the visible area in world coordinates
    BLRect GetVisibleWorldBounds(const Camera &camera, const Viewport &viewport) const;

    // std::vector<std::unique_ptr<RenderStage>> m_stages;
    // Or a more direct approach if stages are fixed:
    // void GeometryPass(RenderContext& context);
    // void LightingPass(RenderContext& context);
    // void PostProcessingPass(RenderContext& context);
    // void UIPass(RenderContext& context);

    // Helper methods for drawing specific parts, called from Execute
    void RenderBoard(
        BLContext &bl_ctx,
        const Board &board,
        const Camera &camera,
        const Viewport &viewport,
        const BLRect &world_view_rect);

    RenderContext *m_renderContext = nullptr; // Store a pointer to the context if needed by multiple methods
    bool m_initialized = false;

    // Cache for loaded font faces
    std::map<std::string, BLFontFace> m_fontFaceCache;
    // Add any other members needed for managing rendering state or resources for the pipeline
};
# Rendering PCB Elements: Plan and Target Files

This section outlines the plan and target files for implementing the rendering of PCB board elements using Blend2D.

## Phase 1: Core Rendering Logic

**Primary Target File:** `src/render/RenderPipeline.cpp`

1.  **Implement `RenderPipeline::RenderBoard()`:**
    *   Verify and apply camera/viewport transformations to the `BLContext`.
    *   Iterate through each `LayerInfo` in `board.layers`.
    *   For each visible layer (`layer.is_visible == true`):
        *   Set the current drawing fill/stroke style on `bl_ctx` using `layer.color`.
        *   **Traces:** Iterate `board.traces`. If on the current layer, call a new helper `RenderTrace(BLContext& bl_ctx, const Trace& trace)` to draw the trace (e.g., `bl_ctx.strokeLine()` or `BLPath` with stroke).
        *   **Vias:** Iterate `board.vias`. If spanning the current layer, call `RenderVia(BLContext& bl_ctx, const Via& via, int current_layer_id)` to draw its pad for this layer (e.g., `bl_ctx.fillCircle()`).
        *   **Arcs:** Iterate `board.arcs`. If on the current layer, call `RenderArc(BLContext& bl_ctx, const Arc& arc)` (e.g., `bl_ctx.strokePath()` with a `BLPath` constructed using `arcTo`).
        *   **Standalone Text Labels:** Iterate `board.standalone_text_labels_`. If on the current layer, call `RenderTextLabel(BLContext& bl_ctx, const TextLabel& label)` (e.g., `bl_ctx.fillUtf8Text()`).
        *   **Components:** Iterate `board.components`. If the component or its features are on the current layer:
            *   Call a new helper `RenderComponent(BLContext& bl_ctx, const Component& component, int current_layer_id)`.
            *   Inside `RenderComponent`:
                *   Render Pins: Iterate `component.pins_`. If a pin is on `current_layer_id`, call `RenderPin(BLContext& bl_ctx, const Pin& pin)`. This will need to handle `PadShape` variants (Circle, Rectangle, Capsule) using appropriate Blend2D drawing calls.
                *   Render Graphical Elements (Silkscreen/Outlines): Iterate `component.graphical_elements_`. If a `LineSegment` is on `current_layer_id`, draw it.
                *   Render Component Text Labels: Iterate `component.text_labels_`. If on `current_layer_id`, draw them.

**Secondary Target File (Recommended Prerequisite/Concurrent):** `src/view/Grid.cpp`

1.  **Adapt `Grid::Render()` Method:**
    *   Modify the existing `Grid::Render(SDL_Renderer* renderer, ...)` method or fully implement the alternative `Grid::Render(BLContext& bl_ctx, ...)` signature.
    *   Change all SDL drawing calls (`SDL_SetRenderDrawColor`, `SDL_RenderLine`, `SDL_RenderPoint`) to their Blend2D equivalents using the passed `bl_ctx` (`bl_ctx.setStrokeStyle`, `bl_ctx.strokeLine`, `bl_ctx.fillCircle` for dots, etc.).
    *   This ensures the grid is rendered into the same Blend2D off-screen buffer as the board elements.

**Data Source Files (Read-Only in this context):**
*   `src/pcb/Board.hpp` (and its `LayerInfo`)
*   `src/pcb/elements/Arc.hpp`
*   `src/pcb/elements/Component.hpp`
*   `src/pcb/elements/Net.hpp` (less direct rendering, but info might be used later)
*   `src/pcb/elements/Pin.hpp` (and its `PadShape`)
*   `src/pcb/elements/TextLabel.hpp`
*   `src/pcb/elements/Trace.hpp`
*   `src/pcb/elements/Via.hpp`
*   `src/view/Camera.hpp` (for transform parameters)
*   `src/view/Viewport.hpp` (for transform parameters)

## Phase 2: Optimizations (Refer to sections below)

Once basic element rendering is functional, apply optimizations such as:
*   Path caching for traces, arcs, and component footprints.
*   Pattern rendering for repetitive pads (if applicable).
*   Adjusting `RenderContext` settings (`OptimizeForStatic`/`OptimizeForInteractive`).
*   Consider advanced layer compositing if performance with many layers becomes an issue.

---

# Original Content Below:
After reviewing the Blend2D demos, there are several powerful techniques we can apply to enhance your PCB rendering system beyond our grid optimizations:

## 1. Caching and Path Optimization (from bl_tiger_demo.cpp)

For rendering PCB traces and complex shapes that don't change frequently:

```cpp
// Add to your PCB rendering class:
struct CachedPath {
    BLPath originalPath;        // Original path data
    BLPath strokedPath;         // Pre-stroked version for fast rendering
    BLStrokeOptions strokeOpts; // Stroke parameters
    bool needsUpdate = true;    // Flag to track if cache is valid
};

// Use in your trace rendering:
if (cachedPath.needsUpdate) {
    // Only compute the stroked path when parameters change
    cachedPath.strokedPath.clear();
    cachedPath.strokedPath.addStrokedPath(cachedPath.originalPath, 
                                         cachedPath.strokeOpts, 
                                         blDefaultApproximationOptions);
    cachedPath.needsUpdate = false;
}

// Then just fill the pre-stroked path instead of stroking each time
ctx.fillPath(cachedPath.strokedPath, traceColor);
```

## 2. Pattern-Based Rendering (from bl_pattern_demo.cpp)

For repetitive elements like component pins or pad arrays:

```cpp
// Create once:
BLPattern pattern(padImage, BL_EXTEND_MODE_REPEAT);

// Use efficiently on each draw:
// Transform the pattern to match component position
pattern.rotate(angleRad);
pattern.translate(tx, ty);
pattern.scale(scale);

// Apply with high performance
ctx.setPatternQuality(BL_PATTERN_QUALITY_BILINEAR);
ctx.fillPath(padOutlinePath, pattern);
```

## 3. Efficient Arc Handling (from bl_elliptic_arc_demo.cpp)

PCB traces often contain arcs, which can be optimized:

```cpp
// For common arc shapes, use a path cache:
// This is much faster than recalculating each frame
BLPath arcPath;
arcPath.reserve(64); // Pre-allocate memory

// Calculate arc path once
arcPath.arcTo(centerX, centerY, radius, radius, 
             startAngle, sweepAngle);

// Reuse for rendering
ctx.strokePath(arcPath, traceColor);
```

## 4. Performance Context Settings (from bl_qt_canvas.cpp)

Add these to your RenderContext:

```cpp
void RenderContext::OptimizeForStatic() {
    // Use these settings when rendering static content
    m_blContext.setCompOp(BL_COMP_OP_SRC_OVER);
    m_blContext.setFillRule(BL_FILL_RULE_NON_ZERO);
    m_blContext.setApproximationOptions(BL_APPROXIMATION_MODE_PRECISION);
}

void RenderContext::OptimizeForInteractive() {
    // Use these for dynamic/interactive content
    m_blContext.setCompOp(BL_COMP_OP_SRC_OVER);
    m_blContext.setApproximationOptions(BL_APPROXIMATION_MODE_SPEED);
}
```

## 5. Render Pipeline Improvements (from bl_paths_demo.cpp)

Update your RenderPipeline to use these techniques:

```cpp
// In RenderPipeline.cpp:
void RenderPipeline::RenderBoard(BLContext& bl_ctx, const Board& board, 
                               const Camera& camera, const Viewport& viewport) {
    // Use the Blend2D transform matrix for camera transformation
    BLMatrix2D transform;
    transform.reset();
    
    // Apply viewport transform
    bl_ctx.save();
    
    // Set up the world transform for correct scaling and panning
    transform.translate(viewport.GetWidth() / 2, viewport.GetHeight() / 2);
    transform.scale(camera.GetZoom());
    transform.rotate(camera.GetRotation());
    transform.translate(-camera.GetPanX(), -camera.GetPanY());
    
    bl_ctx.setMatrix(transform);
    
    // Batch similar operations
    RenderTraces(bl_ctx, board);
    RenderPads(bl_ctx, board);
    RenderComponents(bl_ctx, board);
    
    bl_ctx.restore();
}
```

## 6. Advanced Layer Compositing (from bl_circles_demo.cpp)

For efficient multi-layer rendering:

```cpp
// Render each PCB layer to its own cached image
for (const auto& layer : board.GetLayers()) {
    // Only re-render if layer is dirty or visibility changed
    if (layer.NeedsRender()) {
        BLImage& layerImage = GetLayerImage(layer.GetId());
        BLContext layerCtx(layerImage);
        
        // Clear the layer image
        layerCtx.clearAll();
        
        // Render the layer content
        RenderLayerContent(layerCtx, layer);
        
        // Mark as rendered
        layer.SetNeedsRender(false);
    }
}

// Composite all visible layers with proper blend modes
for (const auto& layer : board.GetLayers()) {
    if (layer.IsVisible()) {
        // Choose appropriate blend mode for this layer type
        BLCompOp compOp = (layer.IsCopper()) 
                          ? BL_COMP_OP_SRC_OVER 
                          : BL_COMP_OP_PLUS;
        
        bl_ctx.setCompOp(compOp);
        bl_ctx.drawImage(GetLayerImage(layer.GetId()));
    }
}
```

## Implementation Recommendations

1. **For immediate performance gains**: Implement the caching strategy for PCB traces and patterns
2. **For PCB-specific rendering**: Add the arc optimization techniques
3. **For system architecture**: Consider restructuring the rendering pipeline to use layer compositing

The key insights from the Blend2D demos are:

1. **Minimize state changes**: Batch similar rendering operations together
2. **Pre-compute when possible**: Calculate static paths once and reuse
3. **Use hardware acceleration efficiently**: Choose the right composition operations
4. **Manage memory thoughtfully**: Reserve path capacity before adding points
5. **Adapt to level of detail**: Use simpler representations at high zoom levels

By incorporating these techniques, your PCB rendering should achieve the same impressive performance we saw with the grid optimizations, even when handling complex boards with thousands of elements.

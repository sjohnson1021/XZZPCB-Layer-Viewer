#pragma once

#include <memory>  // For std::shared_ptr or std::unique_ptr if settings are owned
#include <vector>

#include <blend2d.h>  // Include for BLContext

#include "Camera.hpp"
#include "GridSettings.hpp"


// Forward declarations
class GridSettings;
class Camera;
class Viewport;
// class RenderContext; // No longer needed if BLContext is used directly
// No longer need SDL_Renderer forward declaration

class Grid
{
public:
    // Grid might take a reference to settings, or own them.
    // Using a shared_ptr allows settings to be shared, e.g., with a UI panel.
    explicit Grid(std::shared_ptr<GridSettings> settings);
    ~Grid();

    Grid(const Grid&) = delete;
    Grid& operator=(const Grid&) = delete;
    Grid(Grid&&) = delete;
    Grid& operator=(Grid&&) = delete;

    // The Render method will need information about the current view.
    // This could be Camera & Viewport, or a RenderContext that provides them
    // and also the drawing capabilities (e.g., SDL_Renderer or BLContext).
    void Render(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport) const;
    // Alternate Render signature if using a general RenderContext:
    // void Render(RenderContext& context, const Camera& camera, const Viewport& viewport) const;

    // Returns current effective grid spacings in world units
    // These can be used for displaying measurement information
    struct GridMeasurementInfo {
        float major_spacing {};           // Effective major spacing in world units
        float minor_spacing {};           // Effective minor spacing in world units
        int subdivisions {};              // Effective subdivision count
        bool major_lines_visible {};      // Whether major lines/dots are currently being drawn
        bool minor_lines_visible {};      // Whether minor lines/dots are currently being drawn
        std::string_view unit_string {};  // "mm", "in", etc. based on current unit system
    };

    // Gets measurement info based on current camera/viewport state
    GridMeasurementInfo GetMeasurementInfo(const Camera& camera, const Viewport& viewport) const;

    // Note: Measurement overlay is now rendered by Application layer using ImGui

    // Note: Font scaling is now handled automatically by ImGui's FontGlobalScale
    // No need for manual font management for the measurement overlay

    // Update settings if needed (e.g., if settings are not owned via shared_ptr)
    // void SetSettings(const GridSettings& settings);

    // Calculates effective spacings and subdivision count based on settings and camera zoom.
    void GetEffectiveSpacings(const Camera& camera,
                              float& out_major_spacing_world,  // Output: Effective major spacing in world units
                              float& out_minor_spacing_world,  // Output: Effective minor spacing in world units (derived from major and effective subdivisions)
                              int& out_effective_subdivisions  // Output: Effective number of subdivisions to consider for rendering
    ) const;

private:
    // Settings and old Blend2D font members (kept for backward compatibility with old readout method)
    std::shared_ptr<GridSettings> m_settings_;
    mutable BLFontFace m_font_face_ {};
    mutable BLFont m_font_ {};
    mutable bool m_font_initialized_ = false;
    mutable bool m_font_load_failed_ = false;

    // Helper methods
    void InitializeFont() const;
    static void GetVisibleWorldBounds(const Camera& camera, const Viewport& viewport, Vec2& out_min_world, Vec2& out_max_world);

    void DrawLinesStyle(BLContext& bl_ctx,
                        const Camera& camera,
                        const Viewport& viewport,
                        float major_spacing,
                        float minor_spacing,
                        const Vec2& world_min,
                        const Vec2& world_max,
                        bool actually_draw_major_lines,
                        bool actually_draw_minor_lines) const;
    void DrawDotsStyle(BLContext& bl_ctx,
                       const Camera& camera,
                       const Viewport& viewport,
                       float major_spacing,
                       float minor_spacing,
                       const Vec2& world_min,
                       const Vec2& world_max,
                       bool actually_draw_major_dots,
                       bool actually_draw_minor_dots) const;

    // Calculates line or dot counts to enforce rendering limits
    void
    EnforceRenderingLimits(const Camera& camera, const Viewport& viewport, float spacing, const Vec2& world_min, const Vec2& world_max, int& out_estimated_line_count, bool& out_should_render) const;

    void DrawGridLines(BLContext& bl_ctx,
                       const Camera& camera,
                       const Viewport& viewport,
                       float spacing,
                       const BLRgba32& color,
                       const Vec2& world_min,
                       const Vec2& world_max,
                       bool is_major,
                       float major_spacing_for_axis_check) const;
    void DrawGridDots(BLContext& bl_ctx,
                      const Camera& camera,
                      const Viewport& viewport,
                      float spacing,
                      const BLRgba32& color,
                      const Vec2& world_min,
                      const Vec2& world_max,
                      bool is_major,
                      float major_spacing_for_axis_check) const;

    void DrawAxis(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, const Vec2& world_min, const Vec2& world_max) const;

    // Render measurement readout in the corner of the viewport
    void RenderMeasurementReadout(BLContext& bl_ctx, const Viewport& viewport, const GridMeasurementInfo& info) const;

    // Generate "nice" unit values appropriate for the current unit system
    void GetNiceUnitFactors(std::vector<float>& out_factors) const;

    // Helper methods for drawing, calculating line positions, etc.
    // void DrawLines(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport) const;
    // void DrawDots(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport) const;
    // void CalculateVisibleGrid(const Camera& camera, const Viewport& viewport, ... ) const;
};

#pragma once

#include <memory> // For std::shared_ptr or std::unique_ptr if settings are owned
#include <vector>
#include <blend2d.h> // Include for BLContext
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

    Grid(const Grid &) = delete;
    Grid &operator=(const Grid &) = delete;
    Grid(Grid &&) = delete;
    Grid &operator=(Grid &&) = delete;

    // The Render method will need information about the current view.
    // This could be Camera & Viewport, or a RenderContext that provides them
    // and also the drawing capabilities (e.g., SDL_Renderer or BLContext).
    void Render(BLContext &bl_ctx, const Camera &camera, const Viewport &viewport) const;
    // Alternate Render signature if using a general RenderContext:
    // void Render(RenderContext& context, const Camera& camera, const Viewport& viewport) const;

    // Returns current effective grid spacings in world units
    // These can be used for displaying measurement information
    struct GridMeasurementInfo
    {
        float majorSpacing;     // Effective major spacing in world units
        float minorSpacing;     // Effective minor spacing in world units
        int subdivisions;       // Effective subdivision count
        bool majorLinesVisible; // Whether major lines/dots are currently being drawn
        bool minorLinesVisible; // Whether minor lines/dots are currently being drawn
        const char *unitString; // "mm", "in", etc. based on current unit system
    };

    // Gets measurement info based on current camera/viewport state
    GridMeasurementInfo GetMeasurementInfo(const Camera &camera, const Viewport &viewport) const;

    // Update settings if needed (e.g., if settings are not owned via shared_ptr)
    // void SetSettings(const GridSettings& settings);

    // Calculates effective spacings and subdivision count based on settings and camera zoom.
    void GetEffectiveSpacings(
        const Camera &camera,
        float &outMajorSpacing_world, // Output: Effective major spacing in world units
        float &outMinorSpacing_world, // Output: Effective minor spacing in world units (derived from major and effective subdivisions)
        int &outEffectiveSubdivisions // Output: Effective number of subdivisions to consider for rendering
    ) const;

private:
    std::shared_ptr<GridSettings> m_settings;

    // Font related members
    mutable BLFontFace m_fontFace;
    mutable BLFont m_font;
    mutable bool m_fontInitialized;
    mutable bool m_fontLoadFailed;

    // Helper methods
    void InitializeFont() const; // Made const as it modifies mutable members
    void GetVisibleWorldBounds(const Camera &camera, const Viewport &viewport, Vec2 &outMinWorld, Vec2 &outMaxWorld) const;

    void DrawLinesStyle(
        BLContext &bl_ctx, const Camera &camera, const Viewport &viewport,
        float majorSpacing, float minorSpacing,
        const Vec2 &worldMin, const Vec2 &worldMax,
        bool actuallyDrawMajorLines,
        bool actuallyDrawMinorLines) const;
    void DrawDotsStyle(
        BLContext &bl_ctx, const Camera &camera, const Viewport &viewport,
        float majorSpacing, float minorSpacing,
        const Vec2 &worldMin, const Vec2 &worldMax,
        bool actuallyDrawMajorDots,
        bool actuallyDrawMinorDots) const;

    // Calculates line or dot counts to enforce rendering limits
    void EnforceRenderingLimits(
        const Camera &camera, const Viewport &viewport,
        float spacing, const Vec2 &worldMin, const Vec2 &worldMax,
        int &outEstimatedLineCount, bool &outShouldRender) const;

    void DrawGridLines(BLContext &bl_ctx, const Camera &camera, const Viewport &viewport, float spacing, const GridColor &color, const Vec2 &worldMin, const Vec2 &worldMax, bool isMajor, float majorSpacingForAxisCheck) const;
    void DrawGridDots(BLContext &bl_ctx, const Camera &camera, const Viewport &viewport, float spacing, const GridColor &color, const Vec2 &worldMin, const Vec2 &worldMax, bool isMajor, float majorSpacingForAxisCheck) const;

    void DrawAxis(BLContext &bl_ctx, const Camera &camera, const Viewport &viewport, const Vec2 &worldMin, const Vec2 &worldMax) const;

    // Render measurement readout in the corner of the viewport
    void RenderMeasurementReadout(BLContext &bl_ctx, const Viewport &viewport, const GridMeasurementInfo &info) const;

    // Generate "nice" unit values appropriate for the current unit system
    void GetNiceUnitFactors(std::vector<float> &outFactors) const;

    // Helper methods for drawing, calculating line positions, etc.
    // void DrawLines(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport) const;
    // void DrawDots(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport) const;
    // void CalculateVisibleGrid(const Camera& camera, const Viewport& viewport, ... ) const;
};
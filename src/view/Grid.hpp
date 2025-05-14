#pragma once

#include <memory> // For std::shared_ptr or std::unique_ptr if settings are owned
#include "Camera.hpp"
#include "GridSettings.hpp"

// Forward declarations
class GridSettings;
class Camera;
class Viewport;
class RenderContext; // Or a more specific drawing interface/context
struct SDL_Renderer; // If drawing directly with SDL
// struct BLContext; // If drawing with Blend2D

class Grid {
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
    void Render(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport) const;
    // Alternate Render signature if using a general RenderContext:
    // void Render(RenderContext& context, const Camera& camera, const Viewport& viewport) const;

    // Update settings if needed (e.g., if settings are not owned via shared_ptr)
    // void SetSettings(const GridSettings& settings);

private:
    std::shared_ptr<GridSettings> m_settings;

    // Helper methods
    void GetEffectiveSpacings(const Camera& camera, float& outMajorSpacing, float& outMinorSpacing) const;
    void GetVisibleWorldBounds(const Camera& camera, const Viewport& viewport, Vec2& outMinWorld, Vec2& outMaxWorld) const;
    
    void DrawLinesStyle(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, float majorSpacing, float minorSpacing, const Vec2& worldMin, const Vec2& worldMax) const;
    void DrawDotsStyle(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, float majorSpacing, float minorSpacing, const Vec2& worldMin, const Vec2& worldMax) const;
    
    void DrawGridLines(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, float spacing, const GridColor& color, const Vec2& worldMin, const Vec2& worldMax, bool isMajor, float majorSpacingForAxisCheck) const;
    void DrawGridDots(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, float spacing, const GridColor& color, const Vec2& worldMin, const Vec2& worldMax, bool isMajor, float majorSpacingForAxisCheck) const;

    void DrawAxis(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, const Vec2& worldMin, const Vec2& worldMax) const;

    // Static helper for color conversion
    static void ConvertAndSetSDLColor(SDL_Renderer* renderer, const GridColor& gridColor, float alphaMultiplier = 1.0f);

    // Helper methods for drawing, calculating line positions, etc.
    // void DrawLines(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport) const;
    // void DrawDots(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport) const;
    // void CalculateVisibleGrid(const Camera& camera, const Viewport& viewport, ... ) const;
}; 
#pragma once

#include "utils/Vec2.hpp" // User-adjusted path
// #include "view/Camera.hpp" // Will be removed

class Camera; // Forward declaration

class Viewport {
public:
    Viewport();
    // Viewport might be initialized with screen dimensions
    Viewport(int screenX, int screenY, int screenWidth, int screenHeight);

    void SetDimensions(int x, int y, int width, int height);
    void SetSize(int width, int height);

    int GetX() const { return m_screenX; }
    int GetY() const { return m_screenY; }
    int GetWidth() const { return m_screenWidth; }
    int GetHeight() const { return m_screenHeight; }
    Vec2 GetScreenCenter() const; // Center of the viewport in screen coordinates

    // Coordinate transformation methods
    // These will typically take a Camera reference or its transformation parameters

    // Converts a point from screen coordinates (e.g., mouse position) to world coordinates
    Vec2 ScreenToWorld(const Vec2& screenPoint, const Camera& camera) const;

    // Converts a point from world coordinates to screen coordinates
    Vec2 WorldToScreen(const Vec2& worldPoint, const Camera& camera) const;

    // It can also be useful to convert deltas (vectors rather than points)
    Vec2 ScreenDeltaToWorldDelta(const Vec2& screenDelta, const Camera& camera) const;
    Vec2 WorldDeltaToScreenDelta(const Vec2& worldDelta, const Camera& camera) const;

    // Apply viewport (e.g., glViewport or equivalent for SDL/Blend2D)
    // This might be called by RenderContext or RenderPipeline before drawing to this viewport.
    // void Apply() const;

private:
    int m_screenX;      // Top-left X of the viewport on the screen/window
    int m_screenY;      // Top-left Y of the viewport on the screen/window
    int m_screenWidth;  // Width of the viewport in pixels
    int m_screenHeight; // Height of the viewport in pixels
}; 
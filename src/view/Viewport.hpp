#pragma once

#include "utils/Vec2.hpp" // User-adjusted path
// #include "view/Camera.hpp" // Will be removed

class Camera; // Forward declaration

class Viewport {
public:
    Viewport();
    // Viewport might be initialized with screen dimensions
    Viewport(int screen_x, int screen_y, int screen_width, int screen_height);

    void SetDimensions(int x, int y, int width, int height);
    void SetSize(int width, int height);

    [[nodiscard]] int GetX() const { return m_screen_x_; }
    [[nodiscard]] int GetY() const { return m_screen_y_; }
    [[nodiscard]] int GetWidth() const { return m_screen_width_; }
    [[nodiscard]] int GetHeight() const { return m_screen_height_; }
    [[nodiscard]] Vec2 GetScreenCenter() const;  // Center of the viewport in screen coordinates

    // Coordinate transformation methods
    // These will typically take a Camera reference or its transformation parameters

    // Converts a point from screen coordinates (e.g., mouse position) to world coordinates
    [[nodiscard]] Vec2 ScreenToWorld(const Vec2& screen_point, const Camera& camera) const;

    // Converts a point from world coordinates to screen coordinates
    [[nodiscard]] Vec2 WorldToScreen(const Vec2& world_point, const Camera& camera) const;

    // It can also be useful to convert deltas (vectors rather than points)
    [[nodiscard]] Vec2 ScreenDeltaToWorldDelta(const Vec2& screen_delta, const Camera& camera) const;
    [[nodiscard]] Vec2 WorldDeltaToScreenDelta(const Vec2& world_delta, const Camera& camera) const;

    // Apply viewport (e.g., glViewport or equivalent for SDL/Blend2D)
    // This might be called by RenderContext or RenderPipeline before drawing to this viewport.
    // void Apply() const;

private:
    int m_screen_x_;       // Top-left X of the viewport on the screen/window
    int m_screen_y_;       // Top-left Y of the viewport on the screen/window
    int m_screen_width_;   // Width of the viewport in pixels
    int m_screen_height_;  // Height of the viewport in pixels
}; 
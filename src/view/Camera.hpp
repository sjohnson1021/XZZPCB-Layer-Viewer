#pragma once

// It's common to use a math library for vector/matrix operations.
// For example, GLM. If not using one, we might need simple Vec2/Matrix3x2 types.
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>

#include <blend2d.h>
#include "utils/Vec2.hpp"
// #include "Viewport.hpp" // Will be removed

class Viewport; // Forward declaration

// A simple 2D transformation matrix (like a 3x2 matrix for affine transforms)
// For simplicity, we might just store position, scale, rotation separately for a 2D camera.

class Camera
{
public:
    Camera();

    void SetPosition(const Vec2 &position);
    const Vec2 &GetPosition() const;

    void SetZoom(float zoom); // Zoom level (e.g., 1.0f = normal, >1.0f = zoomed in, <1.0f = zoomed out)
    float GetZoom() const;

    void SetRotation(float angleDegrees); // Rotation in degrees
    float GetRotation() const;

    // Cached rotation values (public getters)
    float GetCachedCosRotation() const { return m_cachedCosRotation; }
    float GetCachedSinRotation() const { return m_cachedSinRotation; }

    // Movement and Zooming
    void Pan(const Vec2 &delta);                            // Move the camera by a delta in world coordinates
    void ZoomAt(const Vec2 &screenPoint, float zoomFactor); // Zoom towards/away from a screen point
    void AdjustZoom(float zoomDelta);                       // Simple zoom adjustment

    // View transformation helpers (can be useful for direct matrix construction or logic)
    Vec2 GetWorldToViewOffset() const;
    float GetWorldToViewScale() const;
    float GetWorldToViewRotation() const;

    void Reset(); // Reset to default position, zoom, rotation

    // Focuses the camera on a given world-space rectangle, adjusting pan and zoom.
    void FocusOnRect(const BLRect &worldRect, const Viewport &viewport, float padding = 0.1f);

    // Dirty flag management
    bool WasViewChangedThisFrame() const { return m_viewChangedThisFrame; }
    void ClearViewChangedFlag() { m_viewChangedThisFrame = false; }

private:
    Vec2 m_position;  // Camera position in world space (center of the view)
    float m_zoom;     // Zoom level. 1.0f is no zoom.
    float m_rotation; // Rotation angle in degrees.

    // Cached trigonometric values for rotation
    float m_cachedCosRotation; // cos(m_rotation in radians)
    float m_cachedSinRotation; // sin(m_rotation in radians)

    bool m_viewChangedThisFrame = true; // Initialize to true for first frame render

    // Helper to update internal state or matrices if we had them
    // void UpdateViewMatrix();
};
#pragma once

// It's common to use a math library for vector/matrix operations.
// For example, GLM. If not using one, we might need simple Vec2/Matrix3x2 types.
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>

// For now, let's assume simple types or define them if needed.
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2 operator/(float scalar) const { return Vec2(x / scalar, y / scalar); }
    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    Vec2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }
    Vec2 operator-() const { return Vec2(-x, -y); } // Unary minus
};

// A simple 2D transformation matrix (like a 3x2 matrix for affine transforms)
// For simplicity, we might just store position, scale, rotation separately for a 2D camera.

class Camera {
public:
    Camera();

    void SetPosition(const Vec2& position);
    const Vec2& GetPosition() const;

    void SetZoom(float zoom); // Zoom level (e.g., 1.0f = normal, >1.0f = zoomed in, <1.0f = zoomed out)
    float GetZoom() const;

    void SetRotation(float angleDegrees); // Rotation in degrees
    float GetRotation() const;

    // Movement and Zooming
    void Pan(const Vec2& delta); // Move the camera by a delta in world coordinates
    void ZoomAt(const Vec2& screenPoint, float zoomFactor); // Zoom towards/away from a screen point
    void AdjustZoom(float zoomDelta); // Simple zoom adjustment

    // View matrix (or equivalent transformation parameters)
    // This would transform world coordinates to view/camera coordinates.
    // For 2D, this might be simple: translate by -Position, scale by Zoom, rotate by -Rotation.
    // glm::mat4 GetViewMatrix() const; // If using GLM and 3D-style matrices
    // Or methods to get combined transform components
    Vec2 GetWorldToViewOffset() const; 
    float GetWorldToViewScale() const;
    float GetWorldToViewRotation() const;

    void Reset(); // Reset to default position, zoom, rotation

private:
    Vec2 m_position;    // Camera position in world space (center of the view)
    float m_zoom;       // Zoom level. 1.0f is no zoom.
    float m_rotation;   // Rotation angle in degrees.

    // Helper to update internal state or matrices if we had them
    // void UpdateViewMatrix(); 
}; 
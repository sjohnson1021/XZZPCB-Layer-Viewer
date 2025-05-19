#include "view/Camera.hpp"
#include <cmath> // For std::pow, std::cos, std::sin, etc. if needed for transformations
#include <algorithm> // For std::min/max

// If using GLM:
// #include <glm/gtc/matrix_transform.hpp>

// A common default zoom value
const float DEFAULT_ZOOM = 1.0f;
const Vec2 DEFAULT_POSITION = {0.0f, 0.0f};
const float DEFAULT_ROTATION = 0.0f;

// Define zoom limits
const float MIN_ZOOM_LEVEL = 0.10f;
const float MAX_ZOOM_LEVEL = 50.0f;

// Define PI if not available from cmath or a math library
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Camera::Camera()
    : m_position(DEFAULT_POSITION)
    , m_zoom(DEFAULT_ZOOM)
    , m_rotation(DEFAULT_ROTATION) {
    // UpdateViewMatrix(); // If we had one
}

void Camera::SetPosition(const Vec2& position) {
    m_position = position;
    // UpdateViewMatrix();
}

const Vec2& Camera::GetPosition() const {
    return m_position;
}

void Camera::SetZoom(float zoom) {
    // Add constraints to zoom if necessary (e.g., min/max zoom)
    float clampedZoom = std::max(MIN_ZOOM_LEVEL, std::min(zoom, MAX_ZOOM_LEVEL));
    if (clampedZoom > 0.0f) { // Zoom must be positive (already ensured by MIN_ZOOM_LEVEL > 0)
        m_zoom = clampedZoom;
        // UpdateViewMatrix();
    }
}

float Camera::GetZoom() const {
    return m_zoom;
}

void Camera::SetRotation(float angleDegrees) {
    m_rotation = angleDegrees;
    // Normalize angle if desired (e.g., to [0, 360) or [-180, 180))
    // UpdateViewMatrix();
}

float Camera::GetRotation() const {
    return m_rotation;
}

void Camera::Pan(const Vec2& delta) {
    // Pan delta is often in screen space and needs to be converted to world space
    // by dividing by zoom and rotating if the camera is rotated.
    // For now, let's assume delta is already in world space for simplicity, adjusted for zoom.
    // A more correct pan would consider rotation:
    // float rad = m_rotation * (M_PI / 180.0f);
    // float cosA = std::cos(-rad); // Inverse rotation
    // float sinA = std::sin(-rad);
    // Vec2 worldDelta = {
    //     (delta.x * cosA - delta.y * sinA) / m_zoom,
    //     (delta.x * sinA + delta.y * cosA) / m_zoom
    // };
    // m_position += worldDelta;

    // Simpler pan for now, assuming delta is scaled screen pixels to world units:
    m_position += delta; 
    // UpdateViewMatrix();
}

void Camera::ZoomAt(const Vec2& screenPoint, float zoomFactor) {
    // This is a common way to zoom: zoom towards a specific point on the screen.
    // 1. Convert screenPoint to world coordinates (before zoom).
    //    This needs the current viewport and camera transform.
    //    Let P_world_mouse be the world coordinate under the mouse.
    // 2. Calculate new position: P_new_cam = P_world_mouse - (P_world_mouse - P_old_cam) * (old_zoom / new_zoom)
    //    Or, equivalently: P_new_cam = P_old_cam + (P_world_mouse - P_old_cam) * (1 - old_zoom / new_zoom)
    // 3. Set new zoom and new position.

    // This is a complex operation that usually involves the Viewport/CoordinateSystem.
    // For now, a simpler zoom is in AdjustZoom. We can implement this fully later.
    // Let's just adjust zoom for now.
    float newZoom = m_zoom * zoomFactor;
    SetZoom(newZoom);
}

void Camera::AdjustZoom(float zoomDelta) {
    // zoomDelta could be an additive factor or a multiplier
    // Assuming it's a multiplier (e.g., 1.1 for 10% zoom in, 0.9 for 10% zoom out)
    SetZoom(m_zoom * zoomDelta);
}

// Example of how a simple 2D view transformation might be derived
Vec2 Camera::GetWorldToViewOffset() const {
    // To transform a world point P_world to view space P_view:
    // 1. Translate by -m_position
    // For rotation around camera center:
    // P_translated = P_world - m_position
    // P_rotated = Rotate(P_translated, -m_rotation)
    // P_view = P_rotated * m_zoom
    // So the offset part for the initial translation is -m_position.
    // However, this function might be interpreted as part of a matrix.
    // If view matrix = Scale * Rotate * Translate, then Translate is by -m_position.
    return Vec2(-m_position.x, -m_position.y);
}

float Camera::GetWorldToViewScale() const {
    return m_zoom;
}

float Camera::GetWorldToViewRotation() const {
    // Rotation for the view transform is typically the negative of camera's world rotation.
    return -m_rotation;
}

void Camera::Reset() {
    m_position = DEFAULT_POSITION;
    m_zoom = DEFAULT_ZOOM;
    m_rotation = DEFAULT_ROTATION;
    // UpdateViewMatrix();
}

// void Camera::UpdateViewMatrix() {
    // If we were using a matrix:
    // m_viewMatrix = glm::mat4(1.0f);
    // m_viewMatrix = glm::translate(m_viewMatrix, glm::vec3(m_position.x, m_position.y, 0.0f)); 
    // m_viewMatrix = glm::scale(m_viewMatrix, glm::vec3(m_zoom, m_zoom, 1.0f));
    // m_viewMatrix = glm::rotate(m_viewMatrix, glm::radians(m_rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    // Then usually invert it for the actual view matrix: m_viewMatrix = glm::inverse(m_viewMatrix);
// } 
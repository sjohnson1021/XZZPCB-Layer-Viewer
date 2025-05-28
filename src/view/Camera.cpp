#include "view/Camera.hpp"
#include "view/Viewport.hpp" // Needed for Camera::FocusOnRect
#include <cmath>             // For std::pow, std::cos, std::sin, etc. if needed for transformations
#include <algorithm>         // For std::min/max

// If using GLM:
// #include <glm/gtc/matrix_transform.hpp>

// A common default zoom value
const float DEFAULT_ZOOM = 3.0f;
const Vec2 DEFAULT_POSITION = {0.0f, 0.0f};
const float DEFAULT_ROTATION = 0.0f;

// Define zoom limits
const float MIN_ZOOM_LEVEL = 0.25f;
const float MAX_ZOOM_LEVEL = 100.0f;

// Define PI if not available from cmath or a math library
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Camera::Camera()
    : m_position(DEFAULT_POSITION), m_zoom(DEFAULT_ZOOM), m_rotation(DEFAULT_ROTATION), m_viewChangedThisFrame(true)
{
    float rad = m_rotation * (static_cast<float>(M_PI) / 180.0f);
    m_cachedCosRotation = std::cos(rad);
    m_cachedSinRotation = std::sin(rad);
    // UpdateViewMatrix(); // If we had one
}

void Camera::SetPosition(const Vec2 &position)
{
    if (m_position.x != position.x || m_position.y != position.y)
    {
        m_position = position;
        m_viewChangedThisFrame = true;
    }
    // UpdateViewMatrix();
}

const Vec2 &Camera::GetPosition() const
{
    return m_position;
}

void Camera::SetZoom(float zoom)
{
    // Add constraints to zoom if necessary (e.g., min/max zoom)
    float clampedZoom = std::max(MIN_ZOOM_LEVEL, std::min(zoom, MAX_ZOOM_LEVEL));
    if (clampedZoom > 0.0f)
    { // Zoom must be positive
        if (m_zoom != clampedZoom)
        {
            m_zoom = clampedZoom;
            m_viewChangedThisFrame = true;
        }
    }
    // UpdateViewMatrix();
}

float Camera::GetZoom() const
{
    return m_zoom;
}

void Camera::SetRotation(float angleDegrees)
{
    if (m_rotation != angleDegrees)
    {
        m_rotation = angleDegrees;
        // Normalize angle if desired (e.g., to [0, 360) or [-180, 180))
        float rad = m_rotation * (static_cast<float>(M_PI) / 180.0f);
        m_cachedCosRotation = std::cos(rad);
        m_cachedSinRotation = std::sin(rad);
        m_viewChangedThisFrame = true;
    }
    // UpdateViewMatrix();
}

float Camera::GetRotation() const
{
    return m_rotation;
}

void Camera::Pan(const Vec2 &delta)
{
    if (delta.x != 0.0f || delta.y != 0.0f)
    {
        m_position -= delta;
        m_viewChangedThisFrame = true;
    }
    // UpdateViewMatrix();
}

void Camera::ZoomAt(const Vec2 &screenPoint, float zoomFactor)
{
    float oldZoom = m_zoom;
    float newZoom = m_zoom * zoomFactor;
    SetZoom(newZoom); // SetZoom will handle clamping and setting m_viewChangedThisFrame if zoom actually changes
    // If SetZoom resulted in a change, m_viewChangedThisFrame is already true.
    // The more complex position adjustment logic would also set m_viewChangedThisFrame if SetPosition is called.
}

void Camera::AdjustZoom(float zoomMultiplier)
{
    if (zoomMultiplier != 1.0f)
    {
        SetZoom(m_zoom * zoomMultiplier); // SetZoom handles the flag
    }
}

// Example of how a simple 2D view transformation might be derived
Vec2 Camera::GetWorldToViewOffset() const
{
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

float Camera::GetWorldToViewScale() const
{
    return m_zoom;
}

float Camera::GetWorldToViewRotation() const
{
    // Rotation for the view transform is typically the negative of camera's world rotation.
    return -m_rotation;
}

void Camera::Reset()
{
    bool changed = false;
    if (m_position.x != DEFAULT_POSITION.x || m_position.y != DEFAULT_POSITION.y)
    {
        m_position = DEFAULT_POSITION;
        changed = true;
    }
    if (m_zoom != DEFAULT_ZOOM)
    {
        m_zoom = DEFAULT_ZOOM;
        changed = true;
    }
    if (m_rotation != DEFAULT_ROTATION)
    {
        m_rotation = DEFAULT_ROTATION;
        float rad = m_rotation * (static_cast<float>(M_PI) / 180.0f);
        m_cachedCosRotation = std::cos(rad);
        m_cachedSinRotation = std::sin(rad);
        changed = true;
    }
    if (changed)
    {
        m_viewChangedThisFrame = true;
    }
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

void Camera::FocusOnRect(const BLRect &worldRect, const Viewport &viewport, float padding)
{
    if (worldRect.w <= 0 || worldRect.h <= 0 || viewport.GetWidth() <= 0 || viewport.GetHeight() <= 0)
    {
        // Cannot focus on an empty rect or with an invalid viewport
        // Optionally, could reset to default view here or log a warning.
        // For now, do nothing to prevent division by zero or nonsensical state.
        return;
    }

    float old_target_pan_x = m_position.x;
    float old_target_pan_y = m_position.y;
    float old_zoom = m_zoom;

    float target_pan_x = static_cast<float>(worldRect.x + worldRect.w / 2.0);
    float target_pan_y = static_cast<float>(worldRect.y + worldRect.h / 2.0);
    // SetPosition will set m_viewChangedThisFrame if position actually changes
    SetPosition({target_pan_x, target_pan_y});

    // Calculate the required zoom to fit the worldRect into the viewport dimensions with padding.
    // The padding is a percentage of the rect's width/height added to each side.
    float padded_rect_width = static_cast<float>(worldRect.w * (1.0f + padding));
    float padded_rect_height = static_cast<float>(worldRect.h * (1.0f + padding));

    // Ensure padded dimensions are not zero to avoid division by zero if original w/h was tiny.
    if (padded_rect_width <= 0)
        padded_rect_width = 1.0f; // Min effective width for zoom calc
    if (padded_rect_height <= 0)
        padded_rect_height = 1.0f; // Min effective height

    float zoom_x = static_cast<float>(viewport.GetWidth()) / padded_rect_width;
    float zoom_y = static_cast<float>(viewport.GetHeight()) / padded_rect_height;

    // Use the smaller of the two zoom factors to ensure the entire rect fits.
    SetZoom(std::max(.001f, std::min(zoom_x, zoom_y)));

    // Rotation is not affected by focusing on a rect, typically.
    // If you want to reset rotation too, you could call SetRotation(DEFAULT_ROTATION);

    // Ensure flag is set if either position or zoom might have changed, even if SetPosition/SetZoom didn't detect a change
    // due to floating point comparisons but the intent was to change.
    // However, SetPosition and SetZoom already compare old/new values.
    // A simpler way: if the function is called, assume a change is intended.
    // But more robust is to check if state actually changed.
    // The individual SetPosition/SetZoom calls should correctly set the flag if they result in a state change.
    // If SetPosition and SetZoom are called but values are identical, they won't set the flag.
    // If this function implies a view change regardless, then set it unconditionally:
    // m_viewChangedThisFrame = true; // Uncomment if FocusOnRect always means a redraw.
    // For now, rely on SetPosition/SetZoom internal logic.
}
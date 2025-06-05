#include "Camera.hpp"

#include <algorithm>  // For std::min/max
#include <cmath>      // For std::pow, std::cos, std::sin, etc. if needed for transformations

#include "Viewport.hpp"  // Needed for Camera::FocusOnRect

// If using GLM:
// #include <glm/gtc/matrix_transform.hpp>

// A common default zoom value
const float kDefaultZoom = 3.0f;
const Vec2 kDefaultPosition = {0.0f, 0.0f};
const float kDefaultRotation = 0.0f;

// Define zoom limits
const float kMinZoomLevel = 0.25f;
const float kMaxZoomLevel = 100.0f;

// Define PI if not available from cmath or a math library
#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

Camera::Camera() : m_position_(kDefaultPosition), m_zoom_(kDefaultZoom), m_rotation_(kDefaultRotation), m_view_changed_this_frame_(true)
{
    float rad = m_rotation_ * (static_cast<float>(M_PI) / 180.0f);
    m_cached_cos_rotation_ = std::cos(rad);
    m_cached_sin_rotation_ = std::sin(rad);
    // UpdateViewMatrix(); // If we had one
}

void Camera::SetPosition(const Vec2& position)
{
    if (m_position_.x_ax != position.x_ax || m_position_.y_ax != position.y_ax) {
        m_position_ = position;
        m_view_changed_this_frame_ = true;
    }
    // UpdateViewMatrix();
}

const Vec2& Camera::GetPosition() const
{
    return m_position_;
}

void Camera::SetZoom(float zoom)
{
    // Add constraints to zoom if necessary (e.g., min/max zoom)
    float clampedZoom = std::max(kMinZoomLevel, std::min(zoom, kMaxZoomLevel));
    if (clampedZoom > 0.0f) {  // Zoom must be positive
        if (m_zoom_ != clampedZoom) {
            m_zoom_ = clampedZoom;
            m_view_changed_this_frame_ = true;
        }
    }
    // UpdateViewMatrix();
}

float Camera::GetZoom() const
{
    return m_zoom_;
}

void Camera::SetRotation(float angle_degrees)
{
    if (m_rotation_ != angle_degrees) {
        m_rotation_ = angle_degrees;
        // Normalize angle if desired (e.g., to [0, 360) or [-180, 180))
        float rad = m_rotation_ * (static_cast<float>(M_PI) / 180.0f);
        m_cached_cos_rotation_ = std::cos(rad);
        m_cached_sin_rotation_ = std::sin(rad);
        m_view_changed_this_frame_ = true;
    }
    // UpdateViewMatrix();
}

float Camera::GetRotation() const
{
    return m_rotation_;
}

void Camera::Pan(const Vec2& delta)
{
    if (delta.x_ax != 0.0f || delta.y_ax != 0.0f) {
        m_position_ -= delta;
        m_view_changed_this_frame_ = true;
    }
    // UpdateViewMatrix();
}

void Camera::ZoomAt(const Vec2& screen_point, float zoom_factor)
{
    float old_zoom = m_zoom_;
    float new_zoom = m_zoom_ * zoom_factor;
    SetZoom(new_zoom);  // SetZoom will handle clamping and setting m_view_changed_this_frame_ if zoom actually changes
    // If SetZoom resulted in a change, m_view_changed_this_frame_ is already true.
    // The more complex position adjustment logic would also set m_view_changed_this_frame_ if SetPosition is called.
}

void Camera::AdjustZoom(float zoom_multiplier)
{
    if (zoom_multiplier != 1.0f) {
        SetZoom(m_zoom_ * zoom_multiplier);  // SetZoom handles the flag
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
    return Vec2(-m_position_.x_ax, -m_position_.y_ax);
}

float Camera::GetWorldToViewScale() const
{
    return m_zoom_;
}

float Camera::GetWorldToViewRotation() const
{
    // Rotation for the view transform is typically the negative of camera's world rotation.
    return -m_rotation_;
}

void Camera::Reset()
{
    bool changed = false;
    if (m_position_.x_ax != kDefaultPosition.x_ax || m_position_.y_ax != kDefaultPosition.y_ax) {
        m_position_ = kDefaultPosition;
        changed = true;
    }
    if (m_zoom_ != kDefaultZoom) {
        m_zoom_ = kDefaultZoom;
        changed = true;
    }
    if (m_rotation_ != kDefaultRotation) {
        m_rotation_ = kDefaultRotation;
        float rad = m_rotation_ * (static_cast<float>(M_PI) / 180.0f);
        m_cached_cos_rotation_ = std::cos(rad);
        m_cached_sin_rotation_ = std::sin(rad);
        changed = true;
    }
    if (changed) {
        m_view_changed_this_frame_ = true;
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

void Camera::FocusOnRect(const BLRect& world_rect, const Viewport& viewport, float padding)
{
    if (world_rect.w <= 0 || world_rect.h <= 0 || viewport.GetWidth() <= 0 || viewport.GetHeight() <= 0) {
        // Cannot focus on an empty rect or with an invalid viewport
        // Optionally, could reset to default view here or log a warning.
        // For now, do nothing to prevent division by zero or nonsensical state.
        return;
    }

    float old_target_pan_x = m_position_.x_ax;
    float old_target_pan_y = m_position_.y_ax;
    float old_zoom = m_zoom_;

    float target_pan_x = static_cast<float>(world_rect.x + world_rect.w / 2.0);
    float target_pan_y = static_cast<float>(world_rect.y + world_rect.h / 2.0);
    // SetPosition will set m_viewChangedThisFrame if position actually changes
    SetPosition({target_pan_x, target_pan_y});

    // Calculate the required zoom to fit the worldRect into the viewport dimensions with padding.
    // The padding is a percentage of the rect's width/height added to each side.
    float padded_rect_width = static_cast<float>(world_rect.w * (1.0f + padding));
    float padded_rect_height = static_cast<float>(world_rect.h * (1.0f + padding));

    // Ensure padded dimensions are not zero to avoid division by zero if original w/h was tiny.
    if (padded_rect_width <= 0)
        padded_rect_width = 1.0f;  // Min effective width for zoom calc
    if (padded_rect_height <= 0)
        padded_rect_height = 1.0f;  // Min effective height

    float zoom_x = static_cast<float>(viewport.GetWidth()) / padded_rect_width;
    float zoom_y = static_cast<float>(viewport.GetHeight()) / padded_rect_height;

    // Use the smaller of the two zoom factors to ensure the entire rect fits.
    SetZoom(std::max(.001f, std::min(zoom_x, zoom_y)));

    SetRotation(kDefaultRotation);
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

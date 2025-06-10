#include "view/Viewport.hpp"

#include "utils/Constants.hpp"
#include "view/Camera.hpp"

Viewport::Viewport() : m_screen_x_(0), m_screen_y_(0), m_screen_width_(0), m_screen_height_(0) {}

Viewport::Viewport(int screenX, int screenY, int screenWidth, int screenHeight) : m_screen_x_(screenX), m_screen_y_(screenY), m_screen_width_(screenWidth), m_screen_height_(screenHeight) {}

void Viewport::SetDimensions(int x, int y, int width, int height)
{
    m_screen_x_ = x;
    m_screen_y_ = y;
    m_screen_width_ = width;
    m_screen_height_ = height;
}

void Viewport::SetSize(int width, int height)
{
    m_screen_width_ = width;
    m_screen_height_ = height;
}

Vec2 Viewport::GetScreenCenter() const
{
    return Vec2(m_screen_x_ + m_screen_width_ / 2.0F, m_screen_y_ + m_screen_height_ / 2.0F);
}

Vec2 Viewport::ScreenToWorld(const Vec2& screen_point, const Camera& camera) const
{
    if (m_screen_width_ <= 0 || m_screen_height_ <= 0 || camera.GetZoom() == 0.0f) {
        // Avoid division by zero or invalid viewport
        return Vec2(0, 0);  // Or camera position, or throw error
    }

    // 1. Normalize screen coordinates to viewport center (0,0), Y down.
    //    Screen coords usually have (0,0) at top-left of viewport, Y down.
    Vec2 point_in_viewport = {
        screen_point.x_ax - (m_screen_x_ + m_screen_width_ / 2.0F),
        screen_point.y_ax - (m_screen_y_ + m_screen_height_ / 2.0F)  // Y is now down, centered
    };

    // 2. Unscale by camera zoom.
    Vec2 point_in_camera_space_no_rotation = point_in_viewport / camera.GetZoom();

    // 3. Unrotate by camera rotation (rotate by A).
    // If camera is rotated by A, world = R(A) * p_cam_space
    // R(A) = [cosA -sinA; sinA cosA]
    // x_rotated = x * cosA - y * sinA
    // y_rotated = x * sinA + y * cosA
    float cosA = camera.GetCachedCosRotation();
    float sinA = camera.GetCachedSinRotation();  // sinA for positive angle A

    Vec2 point_in_camera_space = {point_in_camera_space_no_rotation.x_ax * cosA - point_in_camera_space_no_rotation.y_ax * sinA,
                                  point_in_camera_space_no_rotation.x_ax * sinA + point_in_camera_space_no_rotation.y_ax * cosA};

    // 4. Translate by camera position. (Camera position is Y-Down world)
    Vec2 world_point = point_in_camera_space + camera.GetPosition();

    return world_point;
}

Vec2 Viewport::WorldToScreen(const Vec2& world_point, const Camera& camera) const
{
    if (m_screen_width_ <= 0 || m_screen_height_ <= 0) {
        return Vec2(0, 0);  // Invalid viewport
    }

    // 1. Translate world point relative to camera position. (All Y-Down world)
    Vec2 point_relative_to_camera = world_point - camera.GetPosition();

    // 2. Rotate by negative camera rotation. (This is R(-A))
    // If camera is rotated by angle A, its matrix is [cosA -sinA; sinA cosA]
    // The view transform (inverse rotation) uses R(-A) = [cosA sinA; -sinA cosA]
    // So, x_view = x_world_rel * cosA + y_world_rel * sinA
    //     y_view = -x_world_rel * sinA + y_world_rel * cosA
    float cosA = camera.GetCachedCosRotation();
    float sinA = camera.GetCachedSinRotation();  // This is sin(A) where A is camera's rotation

    Vec2 point_in_camera_axes = {point_relative_to_camera.x_ax * cosA + point_relative_to_camera.y_ax * sinA, -point_relative_to_camera.x_ax * sinA + point_relative_to_camera.y_ax * cosA};

    // 3. Scale by camera zoom.
    Vec2 point_in_view_space = point_in_camera_axes * camera.GetZoom();

    // 4. Convert to screen coordinates (Y down, (0,0) at viewport top-left).
    Vec2 screen_point = {
        (m_screen_x_ + m_screen_width_ / 2.0F) + point_in_view_space.x_ax,
        (m_screen_y_ + m_screen_height_ / 2.0F) + point_in_view_space.y_ax  // Y is now down
    };

    return screen_point;
}

Vec2 Viewport::ScreenDeltaToWorldDelta(const Vec2& screen_delta, const Camera& camera) const
{
    if (camera.GetZoom() == 0.0f) {
        return Vec2(0, 0);
    }
    // Deltas are not affected by translations (camera position or viewport origin).
    // They are affected by scale and rotation.

    // Screen delta Y is already Y-Down.
    Vec2 adjusted_screen_delta = {screen_delta.x_ax, screen_delta.y_ax};

    // Unscale by zoom.
    Vec2 world_delta_no_rotation = adjusted_screen_delta / camera.GetZoom();

    // Unrotate by camera rotation (rotate by -A).
    float cosA = camera.GetCachedCosRotation();
    float sinA = camera.GetCachedSinRotation();  // sinA for positive angle A

    Vec2 world_delta = {world_delta_no_rotation.x_ax * cosA - world_delta_no_rotation.y_ax * sinA, world_delta_no_rotation.x_ax * sinA + world_delta_no_rotation.y_ax * cosA};

    return world_delta;
}

Vec2 Viewport::WorldDeltaToScreenDelta(const Vec2& world_delta, const Camera& camera) const
{
    // Deltas are not affected by translations.
    // Input worldDelta is Y-Down.

    // Rotate by negative camera rotation (R(-A)).
    float cosA = camera.GetCachedCosRotation();
    float sinA = camera.GetCachedSinRotation();  // sinA for positive angle A

    Vec2 screen_delta_no_zoom = {// R(-A) * world_delta
                                 world_delta.x_ax * cosA + world_delta.y_ax * sinA,
                                 -world_delta.x_ax * sinA + world_delta.y_ax * cosA};

    // Scale by zoom.
    Vec2 screen_delta_y_down = screen_delta_no_zoom * camera.GetZoom();

    // Y-axis is already screen Y-down.
    return Vec2(screen_delta_y_down.x_ax, screen_delta_y_down.y_ax);
}

// void Viewport::Apply() const {
//     // Example for OpenGL: glViewport(m_screenX, m_screenY, m_screenWidth, m_screenHeight);
//     // For SDL Renderer, you might set the viewport on the renderer if drawing direct shapes.
//     // For Blend2D, the target BLImage/Surface usually defines the drawing area.
//     // If this viewport corresponds to an ImGui window content region, that sets the bounds.
// }

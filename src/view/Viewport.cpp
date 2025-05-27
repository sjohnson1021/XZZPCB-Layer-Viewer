#include "view/Viewport.hpp"
#include "view/Camera.hpp"
#include <cmath> // For std::cos, std::sin

// Define PI if not available (e.g. from Camera.cpp or a common math header)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Viewport::Viewport()
    : m_screenX(0), m_screenY(0), m_screenWidth(0), m_screenHeight(0) {}

Viewport::Viewport(int screenX, int screenY, int screenWidth, int screenHeight)
    : m_screenX(screenX), m_screenY(screenY), m_screenWidth(screenWidth), m_screenHeight(screenHeight) {}

void Viewport::SetDimensions(int x, int y, int width, int height)
{
    m_screenX = x;
    m_screenY = y;
    m_screenWidth = width;
    m_screenHeight = height;
}

void Viewport::SetSize(int width, int height)
{
    m_screenWidth = width;
    m_screenHeight = height;
}

Vec2 Viewport::GetScreenCenter() const
{
    return Vec2(m_screenX + m_screenWidth / 2.0f, m_screenY + m_screenHeight / 2.0f);
}

Vec2 Viewport::ScreenToWorld(const Vec2 &screenPoint, const Camera &camera) const
{
    if (m_screenWidth <= 0 || m_screenHeight <= 0 || camera.GetZoom() == 0.0f)
    {
        // Avoid division by zero or invalid viewport
        return Vec2(0, 0); // Or camera position, or throw error
    }

    // 1. Normalize screen coordinates to viewport center (0,0) at center, Y up.
    //    Screen coords usually have (0,0) at top-left of viewport, Y down.
    Vec2 pointInViewport = {
        screenPoint.x - (m_screenX + m_screenWidth / 2.0f),
        (m_screenY + m_screenHeight / 2.0f) - screenPoint.y // Flip Y to make Y point upwards
    };

    // 2. Unscale by camera zoom.
    Vec2 pointInCameraSpaceNoRotation = pointInViewport / camera.GetZoom();

    // 3. Unrotate by camera rotation.
    float cosA = camera.GetCachedCosRotation();
    float sinA = camera.GetCachedSinRotation();

    Vec2 pointInCameraSpace = {
        pointInCameraSpaceNoRotation.x * cosA - pointInCameraSpaceNoRotation.y * sinA,
        pointInCameraSpaceNoRotation.x * sinA + pointInCameraSpaceNoRotation.y * cosA};

    // 4. Translate by camera position.
    Vec2 worldPoint = pointInCameraSpace + camera.GetPosition();

    return worldPoint;
}

Vec2 Viewport::WorldToScreen(const Vec2 &worldPoint, const Camera &camera) const
{
    if (m_screenWidth <= 0 || m_screenHeight <= 0)
    {
        return Vec2(0, 0); // Invalid viewport
    }

    // 1. Translate world point relative to camera position.
    Vec2 pointRelativeToCamera = worldPoint - camera.GetPosition();

    // 2. Rotate by negative camera rotation.
    float cosA = camera.GetCachedCosRotation();
    float sinA = -camera.GetCachedSinRotation();

    Vec2 pointInCameraAxes = {
        pointRelativeToCamera.x * cosA - pointRelativeToCamera.y * sinA,
        pointRelativeToCamera.x * sinA + pointRelativeToCamera.y * cosA};

    // 3. Scale by camera zoom.
    Vec2 pointInViewSpace = pointInCameraAxes * camera.GetZoom();

    // 4. Convert to screen coordinates (Y down, (0,0) at viewport top-left).
    Vec2 screenPoint = {
        (m_screenX + m_screenWidth / 2.0f) + pointInViewSpace.x,
        (m_screenY + m_screenHeight / 2.0f) - pointInViewSpace.y // Flip Y back
    };

    return screenPoint;
}

Vec2 Viewport::ScreenDeltaToWorldDelta(const Vec2 &screenDelta, const Camera &camera) const
{
    if (camera.GetZoom() == 0.0f)
    {
        return Vec2(0, 0);
    }
    // Deltas are not affected by translations (camera position or viewport origin).
    // They are affected by scale and rotation.

    // Adjust for Y-axis direction if screen Y is downwards.
    // (If screenPoint.y increases downwards, a positive screenDelta.y means moving down on screen).
    // Our world space has Y increasing upwards.
    Vec2 adjustedScreenDelta = {screenDelta.x, -screenDelta.y}; // Assuming screen Y is down

    // Unscale by zoom.
    Vec2 worldDeltaNoRotation = adjustedScreenDelta / camera.GetZoom();

    // Unrotate by camera rotation (rotate by camera's positive rotation).
    float cosA = camera.GetCachedCosRotation();
    float sinA = camera.GetCachedSinRotation();

    Vec2 worldDelta = {
        worldDeltaNoRotation.x * cosA - worldDeltaNoRotation.y * sinA,
        worldDeltaNoRotation.x * sinA + worldDeltaNoRotation.y * cosA};

    return worldDelta;
}

Vec2 Viewport::WorldDeltaToScreenDelta(const Vec2 &worldDelta, const Camera &camera) const
{
    // Deltas are not affected by translations.

    // Rotate by negative camera rotation.
    float cosA = camera.GetCachedCosRotation();
    float sinA = -camera.GetCachedSinRotation();

    Vec2 screenDeltaNoRotationNoZoom = {
        worldDelta.x * cosA - worldDelta.y * sinA,
        worldDelta.x * sinA + worldDelta.y * cosA};

    // Scale by zoom.
    Vec2 screenDeltaYUp = screenDeltaNoRotationNoZoom * camera.GetZoom();

    // Adjust for Y-axis direction (screen Y down).
    return Vec2(screenDeltaYUp.x, -screenDeltaYUp.y);
}

// void Viewport::Apply() const {
//     // Example for OpenGL: glViewport(m_screenX, m_screenY, m_screenWidth, m_screenHeight);
//     // For SDL Renderer, you might set the viewport on the renderer if drawing direct shapes.
//     // For Blend2D, the target BLImage/Surface usually defines the drawing area.
//     // If this viewport corresponds to an ImGui window content region, that sets the bounds.
// }
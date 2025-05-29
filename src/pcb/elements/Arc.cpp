#include "pcb/elements/Arc.hpp"
#include "pcb/elements/Component.hpp" // For parentComponent, though not used by Arc
#include "utils/GeometryUtils.hpp"    // For geometric calculations
#include <blend2d.h>
#include <sstream>
#include <cmath>     // For M_PI, cos, sin, etc.
#include <algorithm> // For std::min/max if needed for tighter bbox

// Define M_PI if not already available from cmath on all compilers/platforms
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

BLRect Arc::getBoundingBox(const Component * /*parentComponent*/) const
{
    // Placeholder: More complex for an arc. For now, a box around the circle containing the arc.
    // A tighter bounding box would consider start/end angles.
    // This simple version assumes the arc could be a full circle for bounding box purposes.
    double r = radius + thickness / 2.0;
    return BLRect(cx - r, cy - r, r * 2, r * 2);
    // TODO: Implement a tighter bounding box calculation based on start/end angles.
}

bool Arc::isHit(const Vec2 &worldMousePos, float tolerance, const Component * /*parentComponent*/) const
{
    // 1. Radial Check
    double dx = worldMousePos.x - cx;
    double dy = worldMousePos.y - cy;
    double dist_sq = dx * dx + dy * dy;

    double r_outer = radius + thickness / 2.0 + tolerance;
    double r_inner = radius - thickness / 2.0 - tolerance;
    // Ensure r_inner is not negative, which could happen for thick arcs or large tolerance
    if (r_inner < 0)
        r_inner = 0;

    if (dist_sq > r_outer * r_outer || dist_sq < r_inner * r_inner)
    {
        return false; // Point is outside the annulus (ring shape of the arc)
    }

    // 2. Angular Check
    // Calculate the angle of the mouse position relative to the arc's center
    // std::atan2 returns radians in the range [-PI, PI]
    double angle_rad_mouse_to_center = std::atan2(dy, dx);

    // GeometryUtils::isAngleBetween expects start and end angles in degrees
    // and the query angle in radians.
    return GeometryUtils::isAngleBetween(angle_rad_mouse_to_center, start_angle, end_angle);
}

std::string Arc::getInfo(const Component * /*parentComponent*/) const
{
    std::ostringstream oss;
    oss << "Arc\n";
    oss << "Layer: " << m_layerId << "\n";
    if (m_netId != -1)
    {
        oss << "Net ID: " << m_netId << "\n";
    }
    oss << "Center: (" << cx << ", " << cy << ")\n";
    oss << "Radius: " << radius << ", Thickness: " << thickness << "\n";
    oss << "Angles: " << start_angle << " to " << end_angle << " deg";
    return oss.str();
}

void Arc::translate(double dx, double dy)
{
    cx += dx;
    cy += dy;
    // Radius, angles, thickness are not affected by translation
}
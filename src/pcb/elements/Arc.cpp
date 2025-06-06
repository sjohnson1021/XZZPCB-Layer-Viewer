#include "pcb/elements/Arc.hpp"

#include <algorithm>  // For std::min/max if needed for tighter bbox
#include <cmath>      // For M_PI, cos, sin, etc.
#include <sstream>

#include <blend2d.h>

#include "pcb/elements/Component.hpp"  // For parentComponent, though not used by Arc
#include "utils/Constants.hpp"         // For kPi
#include "utils/GeometryUtils.hpp"     // For geometric calculations

BLRect Arc::GetBoundingBox(const Component* /*parent_component*/) const
{
    // Placeholder: More complex for an arc. For now, a box around the circle containing the arc.
    // A tighter bounding box would consider start/end angles.
    // This simple version assumes the arc could be a full circle for bounding box purposes.
    double r = radius + thickness / 2.0;
    return BLRect(center.x_ax - r, center.y_ax - r, r * 2, r * 2);
    // TODO: Implement a tighter bounding box calculation based on start/end angles.
}

bool Arc::IsHit(const Vec2& world_mouse_pos, float tolerance, const Component* /*parent_component*/) const
{
    // 1. Radial Check
    double dist_x = world_mouse_pos.x_ax - center.x_ax;
    double dist_y = world_mouse_pos.y_ax - center.y_ax;
    double dist_sq = dist_x * dist_x + dist_y * dist_y;

    double r_outer = radius + thickness / 2.0 + tolerance;
    double r_inner = radius - thickness / 2.0 - tolerance;
    // Ensure r_inner is not negative, which could happen for thick arcs or large tolerance
    if (r_inner < 0)
        r_inner = 0;

    if (dist_sq > r_outer * r_outer || dist_sq < r_inner * r_inner) {
        return false;  // Point is outside the annulus (ring shape of the arc)
    }

    // 2. Angular Check
    // Calculate the angle of the mouse position relative to the arc's center
    // std::atan2 returns radians in the range [-PI, PI]
    double const kAngleRadMouseToCenter = std::atan2(dist_y, dist_x);

    // geometry_utils::::isAngleBetween expects start and end angles in degrees
    // and the query angle in radians.
    return geometry_utils::IsAngleBetween(kAngleRadMouseToCenter, start_angle, end_angle);
}

std::string Arc::GetInfo(const Component* /*parent_component*/) const
{
    std::ostringstream oss;
    oss << "Arc\n";
    oss << "Layer: " << GetLayerId() << "\n";
    if (GetNetId() != -1) {
        oss << "Net ID: " << GetNetId() << "\n";
    }
    oss << "Center: (" << center.x_ax << ", " << center.y_ax << ")\n";
    oss << "Radius: " << radius << ", Thickness: " << thickness << "\n";
    oss << "Angles: " << start_angle << " to " << end_angle << " deg";
    return oss.str();
}

void Arc::Translate(double dist_x, double dist_y)
{
    center.x_ax += dist_x;
    center.y_ax += dist_y;
    // Radius, angles, thickness are not affected by translation
}

void Arc::Mirror(double center_axis)
{
    // Mirror the arc center around the vertical axis
    center.x_ax = 2 * center_axis - center.x_ax;

    // For horizontal mirroring, we need to transform the angles
    // When mirroring horizontally, an arc that goes from start_angle to end_angle
    // becomes an arc that goes from (180° - end_angle) to (180° - start_angle)
    // This preserves the arc's shape and direction relative to the mirrored coordinate system

    double original_start = start_angle;
    double original_end = end_angle;

    start_angle = 180.0 - original_end;
    end_angle = 180.0 - original_start;

    // Normalize angles to [0, 360) range
    while (start_angle < 0) start_angle += 360.0;
    while (end_angle < 0) end_angle += 360.0;
    while (start_angle >= 360.0) start_angle -= 360.0;
    while (end_angle >= 360.0) end_angle -= 360.0;

    // Y coordinate, radius, and thickness remain unchanged for horizontal mirroring
}

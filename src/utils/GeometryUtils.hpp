#pragma once
#include <algorithm>  // For std::min, std::max
#include <cmath>      // For std::sqrt, std::fmod, std::atan2, M_PI (if needed)
#include <vector>     // For std::vector

#include "Vec2.hpp"  // For Vec2 struct

// Forward declare Blend2D types if you only pass pointers/references
// or include the necessary Blend2D headers directly.
// For simplicity here, assuming BLPoint for Vec2 might be an option or use your own Vec2.
// struct BLPoint; // If using BLPoint instead of Vec2
// struct BLBox;

namespace geometry_utils
{

const double kPi = 3.14159265358979323846;  // Or M_PI if available and preferred

// --- Angle Utilities (from previous discussion) ---
inline double NormalizeAngleDegrees(double angle)
{
    angle = std::fmod(angle, 360.0);
    if (angle < 0) {
        angle += 360.0;
    }
    return angle;
}

inline bool IsAngleBetween(double test_angle_rad, double min_angle_deg, double max_angle_deg)
{
    double const kTestAngleDeg = NormalizeAngleDegrees(test_angle_rad * (180.0 / kPi));
    double const kMinAngleDeg = NormalizeAngleDegrees(min_angle_deg);
    double const kMaxAngleDeg = NormalizeAngleDegrees(max_angle_deg);

    if (kMinAngleDeg <= kMaxAngleDeg) {  // Normal case
        return kTestAngleDeg >= kMinAngleDeg && kTestAngleDeg <= kMaxAngleDeg;
    }

    // Wrap-around case (e.g., 330 to 30 degrees)
    return kTestAngleDeg >= kMinAngleDeg || kTestAngleDeg <= kMaxAngleDeg;
}

// --- Point-Shape Distance/Containment ---

// Squared distance from point p to line segment (a, b)
inline double DistSqPointToSegment(const Vec2& pnt, const Vec2& seg_start, const Vec2& seg_end)
{
    Vec2 const kSegDelta = seg_end - seg_start;
    Vec2 const kPntDelta = pnt - seg_start;
    double const kSegLenSq = kSegDelta.LengthSquared();
    if (kSegLenSq == 0.0) {
        return kPntDelta.LengthSquared();  // a and b are the same point
    }

    double proj_ratio_clamped = kPntDelta.Dot(kSegDelta) / kSegLenSq;
    proj_ratio_clamped = std::max(0.0, std::min(1.0, proj_ratio_clamped));  // Clamp projection ratio to [0, 1]
    Vec2 const kProjection = seg_start + (kSegDelta * proj_ratio_clamped);
    return (pnt - kProjection).LengthSquared();
}

// Hit test for a line segment with thickness
inline bool IsPointNearLineSegment(const Vec2& world_mouse_pos,
                                   const Vec2& pnt1,  // Segment start (world coords)
                                   const Vec2& pnt2,  // Segment end (world coords)
                                   double thickness,  // Thickness of the line
                                   double tolerance)
{
    if (thickness <= 0) {
        thickness = 1.0;  // Ensure some minimal thickness for hit
    }
    double const kEffectiveRadius = (thickness / 2.0) + tolerance;
    return DistSqPointToSegment(world_mouse_pos, pnt1, pnt2) <= (kEffectiveRadius * kEffectiveRadius);
}

// Hit test for a circle
inline bool IsPointInCircle(const Vec2& world_mouse_pos,
                            const Vec2& center,  // Circle center (world coords)
                            double radius,
                            double tolerance)
{
    double const kEffectiveRadius = radius + tolerance;
    return (world_mouse_pos - center).LengthSquared() <= (kEffectiveRadius * kEffectiveRadius);
}

// Hit test for a polygon (ray casting)
// Ensure polygonVertices are in world coordinates
inline bool IsPointInPolygon(const Vec2& world_mouse_pos, const std::vector<Vec2>& polygon_vertices, double tolerance = 0.0)
{
    unsigned long const kNumVertices = polygon_vertices.size();
    if (kNumVertices < 3) {
        return false;
    }

    // 1. Bounding box pre-check (optional optimization, BLBox might do this)
    // double minX = polygonVertices[0].x, maxX = polygonVertices[0].x;
    // double minY = polygonVertices[0].y, maxY = polygonVertices[0].y;
    // for (int i = 1; i < n; ++i) {
    //     minX = std::min(minX, polygonVertices[i].x);
    //     maxX = std::max(maxX, polygonVertices[i].x);
    //     minY = std::min(minY, polygonVertices[i].y);
    //     maxY = std::max(maxY, polygonVertices[i].y);
    // }
    // if (worldMousePos.x < minX - tolerance || worldMousePos.x > maxX + tolerance ||
    //     worldMousePos.y < minY - tolerance || worldMousePos.y > maxY + tolerance) {
    //     return false;
    // }

    // 2. Ray Casting
    bool inside = false;
    Vec2 pnt1 = polygon_vertices[kNumVertices - 1];  // Start with the last vertex
    for (int i = 0; i < kNumVertices; ++i) {
        Vec2 const kPnt2 = polygon_vertices[i];
        if (((kPnt2.y <= world_mouse_pos.y && world_mouse_pos.y < pnt1.y) || (pnt1.y <= world_mouse_pos.y && world_mouse_pos.y < kPnt2.y)) &&
            (world_mouse_pos.x < (pnt1.x - kPnt2.x) * (world_mouse_pos.y - kPnt2.y) / (pnt1.y - kPnt2.y) + kPnt2.x)) {
            inside = !inside;
        }
        pnt1 = kPnt2;
    }

    // 3. Tolerance check (if not inside, check distance to edges)
    // This makes the function more complex. A simpler way for polygons is often to
    // use the basic "isPointInPolygon" and then, if a "selectable outline" is desired,
    // also test against the line segments forming the polygon's edges.
    if (inside) {
        return true;
    }

    if (tolerance > 0.001) {  // Only check edges if tolerance is meaningful
        for (int i = 0; i < kNumVertices; ++i) {
            Vec2 const kEdgeP1 = polygon_vertices[i];
            Vec2 const kEdgeP2 = polygon_vertices[(i + 1) % kNumVertices];  // Next vertex, wraps around
            if (DistSqPointToSegment(world_mouse_pos, kEdgeP1, kEdgeP2) <= tolerance * tolerance) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace geometry_utils

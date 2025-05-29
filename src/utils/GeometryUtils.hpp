#pragma once
#include "utils/Vec2.hpp" // For Vec2 struct
#include <vector>
#include <cmath>     // For std::sqrt, std::fmod, std::atan2, M_PI (if needed)
#include <algorithm> // For std::min, std::max

// Forward declare Blend2D types if you only pass pointers/references
// or include the necessary Blend2D headers directly.
// For simplicity here, assuming BLPoint for Vec2 might be an option or use your own Vec2.
// struct BLPoint; // If using BLPoint instead of Vec2
// struct BLBox;

namespace GeometryUtils
{

    const double PI = 3.14159265358979323846; // Or M_PI if available and preferred

    // --- Angle Utilities (from previous discussion) ---
    inline double normalizeAngleDegrees(double angle)
    {
        angle = std::fmod(angle, 360.0);
        if (angle < 0)
        {
            angle += 360.0;
        }
        return angle;
    }

    inline bool isAngleBetween(double query_angle_rad, double start_angle_deg, double end_angle_deg)
    {
        double query_angle_deg = normalizeAngleDegrees(query_angle_rad * (180.0 / PI));
        double sa_norm = normalizeAngleDegrees(start_angle_deg);
        double ea_norm = normalizeAngleDegrees(end_angle_deg);

        if (sa_norm <= ea_norm)
        { // Normal case
            return query_angle_deg >= sa_norm && query_angle_deg <= ea_norm;
        }
        else
        { // Wrap-around case (e.g., 330 to 30 degrees)
            return query_angle_deg >= sa_norm || query_angle_deg <= ea_norm;
        }
    }

    // --- Point-Shape Distance/Containment ---

    // Squared distance from point p to line segment (a, b)
    inline double distSqPointToSegment(const Vec2 &p, const Vec2 &a, const Vec2 &b)
    {
        Vec2 ab = b - a;
        Vec2 ap = p - a;
        double l2 = ab.lengthSquared();
        if (l2 == 0.0)
            return ap.lengthSquared(); // a and b are the same point

        double t = ap.dot(ab) / l2;
        t = std::max(0.0, std::min(1.0, t)); // Clamp t to [0, 1]
        Vec2 projection = a + ab * t;
        return (p - projection).lengthSquared();
    }

    // Hit test for a line segment with thickness
    inline bool isPointNearLineSegment(
        const Vec2 &worldMousePos,
        const Vec2 &p1,   // Segment start (world coords)
        const Vec2 &p2,   // Segment end (world coords)
        double thickness, // Thickness of the line
        double tolerance)
    {
        if (thickness <= 0)
            thickness = 1.0; // Ensure some minimal thickness for hit
        double effective_radius = thickness / 2.0 + tolerance;
        return distSqPointToSegment(worldMousePos, p1, p2) <= (effective_radius * effective_radius);
    }

    // Hit test for a circle
    inline bool isPointInCircle(
        const Vec2 &worldMousePos,
        const Vec2 &center, // Circle center (world coords)
        double radius,
        double tolerance)
    {
        double effective_radius = radius + tolerance;
        return (worldMousePos - center).lengthSquared() <= (effective_radius * effective_radius);
    }

    // Hit test for a polygon (ray casting)
    // Ensure polygonVertices are in world coordinates
    inline bool isPointInPolygon(const Vec2 &worldMousePos, const std::vector<Vec2> &polygonVertices, double tolerance = 0.0)
    {
        int n = polygonVertices.size();
        if (n < 3)
            return false;

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
        Vec2 p1 = polygonVertices[n - 1]; // Start with the last vertex
        for (int i = 0; i < n; ++i)
        {
            Vec2 p2 = polygonVertices[i];
            if (((p2.y <= worldMousePos.y && worldMousePos.y < p1.y) || (p1.y <= worldMousePos.y && worldMousePos.y < p2.y)) &&
                (worldMousePos.x < (p1.x - p2.x) * (worldMousePos.y - p2.y) / (p1.y - p2.y) + p2.x))
            {
                inside = !inside;
            }
            p1 = p2;
        }

        // 3. Tolerance check (if not inside, check distance to edges)
        // This makes the function more complex. A simpler way for polygons is often to
        // use the basic "isPointInPolygon" and then, if a "selectable outline" is desired,
        // also test against the line segments forming the polygon's edges.
        if (inside)
            return true;

        if (tolerance > 0.001)
        { // Only check edges if tolerance is meaningful
            for (int i = 0; i < n; ++i)
            {
                Vec2 edge_p1 = polygonVertices[i];
                Vec2 edge_p2 = polygonVertices[(i + 1) % n]; // Next vertex, wraps around
                if (distSqPointToSegment(worldMousePos, edge_p1, edge_p2) <= tolerance * tolerance)
                {
                    return true;
                }
            }
        }
        return false;
    }

} // namespace GeometryUtils
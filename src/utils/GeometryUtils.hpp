#pragma once
#include <algorithm>  // For std::min, std::max
#include <cmath>      // For std::sqrt, std::fmod, std::atan2, M_PI (if needed)
#include <vector>     // For std::vector

#ifdef __SSE__
#include <immintrin.h>  // For SSE/AVX vectorization
#endif

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
        if (((kPnt2.y_ax <= world_mouse_pos.y_ax && world_mouse_pos.y_ax < pnt1.y_ax) || (pnt1.y_ax <= world_mouse_pos.y_ax && world_mouse_pos.y_ax < kPnt2.y_ax)) &&
            (world_mouse_pos.x_ax < (pnt1.x_ax - kPnt2.x_ax) * (world_mouse_pos.y_ax - kPnt2.y_ax) / (pnt1.y_ax - kPnt2.y_ax) + kPnt2.x_ax)) {
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

// --- Vectorized Math Operations for Performance ---

// Vectorized distance calculation for multiple points
inline void BatchDistanceSquared(const std::vector<Vec2>& points, const Vec2& reference,
                                std::vector<float>& results) {
    results.resize(points.size());

    // Process 4 points at once using SSE if available
    #ifdef __SSE__
    const int vectorSize = 4;
    const int vectorizedSize = static_cast<int>(points.size()) / vectorSize * vectorSize;

    __m128 refX = _mm_set1_ps(static_cast<float>(reference.x_ax));
    __m128 refY = _mm_set1_ps(static_cast<float>(reference.y_ax));

    for (int i = 0; i < vectorizedSize; i += vectorSize) {
        // Load 4 points
        float xBuffer[4], yBuffer[4];
        for (int j = 0; j < vectorSize; j++) {
            xBuffer[j] = static_cast<float>(points[i+j].x_ax);
            yBuffer[j] = static_cast<float>(points[i+j].y_ax);
        }

        __m128 ptsX = _mm_loadu_ps(xBuffer);
        __m128 ptsY = _mm_loadu_ps(yBuffer);

        // Calculate differences
        __m128 diffX = _mm_sub_ps(ptsX, refX);
        __m128 diffY = _mm_sub_ps(ptsY, refY);

        // Square differences
        __m128 sqrX = _mm_mul_ps(diffX, diffX);
        __m128 sqrY = _mm_mul_ps(diffY, diffY);

        // Sum squares
        __m128 distSqr = _mm_add_ps(sqrX, sqrY);

        // Store results
        float resultBuffer[4];
        _mm_storeu_ps(resultBuffer, distSqr);

        for (int j = 0; j < vectorSize; j++) {
            results[i+j] = resultBuffer[j];
        }
    }

    // Handle remaining points
    for (size_t i = vectorizedSize; i < points.size(); i++) {
        float dx = static_cast<float>(points[i].x_ax - reference.x_ax);
        float dy = static_cast<float>(points[i].y_ax - reference.y_ax);
        results[i] = dx*dx + dy*dy;
    }
    #else
    // Fallback for non-SSE platforms
    for (size_t i = 0; i < points.size(); i++) {
        float dx = static_cast<float>(points[i].x_ax - reference.x_ax);
        float dy = static_cast<float>(points[i].y_ax - reference.y_ax);
        results[i] = dx*dx + dy*dy;
    }
    #endif
}

// Fast squared distance (avoids sqrt and abs)
inline float FastDistanceSquared(const Vec2& a, const Vec2& b) {
    float dx = static_cast<float>(a.x_ax - b.x_ax);
    float dy = static_cast<float>(a.y_ax - b.y_ax);
    
    // Direct squared distance - fastest possible distance metric
    // No sqrt, no abs, just pure squared distance
    return dx*dx + dy*dy;
}

// Fast approximate distance (avoids sqrt)
inline float FastDistance(const Vec2& a, const Vec2& b) {
    float dx = static_cast<float>(std::abs(a.x_ax - b.x_ax));
    float dy = static_cast<float>(std::abs(a.y_ax - b.y_ax));

    // Use Manhattan distance approximation for very fast distance checks
    // This is about 3x faster than sqrt and good enough for many use cases
    return dx + dy;
}

// Fast approximate distance with better accuracy than Manhattan
inline float FastDistanceApprox(const Vec2& a, const Vec2& b) {
    float dx = static_cast<float>(std::abs(a.x_ax - b.x_ax));
    float dy = static_cast<float>(std::abs(a.y_ax - b.y_ax));

    // Octagonal approximation - much faster than sqrt, more accurate than Manhattan
    float min_val = std::min(dx, dy);
    float max_val = std::max(dx, dy);
    return max_val + 0.4142135f * min_val;  // 0.4142135 ≈ sqrt(2) - 1
}

}  // namespace geometry_utils

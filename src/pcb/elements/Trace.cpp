#include "pcb/elements/Trace.hpp"
#include "pcb/elements/Component.hpp" // For parentComponent, though not used by Trace
#include "utils/GeometryUtils.hpp"    // For geometric calculations
#include <blend2d.h>
#include <sstream>   // For getInfo
#include <algorithm> // For std::min/max
#include <cmath>     // For sqrt, fabs for more advanced hit detection if needed

BLRect Trace::getBoundingBox(const Component * /*parentComponent*/) const
{
    // Calculate bounding box of the trace segment, considering trace width
    double minX = std::min(x1, x2) - width / 2.0;
    double maxX = std::max(x1, x2) + width / 2.0;
    double minY = std::min(y1, y2) - width / 2.0;
    double maxY = std::max(y1, y2) + width / 2.0;
    return BLRect(minX, minY, maxX - minX, maxY - minY);
}

bool Trace::isHit(const Vec2 &worldMousePos, float tolerance, const Component * /*parentComponent*/) const
{
    // Use GeometryUtils for hit detection
    return GeometryUtils::isPointNearLineSegment(
        worldMousePos,
        Vec2(x1, y1), // Start point of the trace
        Vec2(x2, y2), // End point of the trace
        width,        // Thickness of the trace
        static_cast<double>(tolerance));
}

std::string Trace::getInfo(const Component * /*parentComponent*/) const
{
    std::ostringstream oss;
    oss << "Trace\n";
    oss << "Layer: " << m_layerId << "\n";
    if (m_netId != -1)
    {
        oss << "Net ID: " << m_netId << "\n";
    }
    oss << "Width: " << width << "\n";
    oss << "From: (" << x1 << ", " << y1 << ")\n";
    oss << "To: (" << x2 << ", " << y2 << ")";
    return oss.str();
}

void Trace::translate(double dx, double dy)
{
    x1 += dx;
    y1 += dy;
    x2 += dx;
    y2 += dy;
    // Width is not affected by translation
}
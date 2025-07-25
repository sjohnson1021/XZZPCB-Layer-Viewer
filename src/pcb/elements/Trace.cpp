#include "Trace.hpp"

#include <sstream>
#include <string>

#include <blend2d.h>

#include "../Board.hpp"             // For Board class and Net access
#include "Component.hpp"            // For parentComponent, though not used by Trace
#include "utils/GeometryUtils.hpp"  // For geometric calculations

BLRect Trace::GetBoundingBox(const Component* /*parentComponent*/) const
{
    // Calculate bounding box of the trace segment, considering trace width
    double min_x = std::min(x1, x2) - (width / 2.0);
    double max_x = std::max(x1, x2) + (width / 2.0);
    double min_y = std::min(y1, y2) - (width / 2.0);
    double max_y = std::max(y1, y2) + (width / 2.0);
    return BLRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

bool Trace::IsHit(const Vec2& world_mouse_pos, float tolerance, const Component* /*parentComponent*/) const
{
    // Use geometry_utils:: for hit detection
    return geometry_utils::IsPointNearLineSegment(world_mouse_pos,
                                                  Vec2(x1, y1),  // Start point of the trace
                                                  Vec2(x2, y2),  // End point of the trace
                                                  width,         // Thickness of the trace
                                                  static_cast<double>(tolerance));
}

std::string Trace::GetInfo(const Component* /*parentComponent*/, const Board* board) const
{
    std::ostringstream oss = {};
    oss << "Trace\n";
    oss << "Layer: " << GetLayerId() << "\n";

    // Enhanced net information display
    if (GetNetId() != -1) {
        if (board) {
            const Net* net = board->GetNetById(GetNetId());
            if (net) {
                oss << "Net: " << (net->GetName().empty() ? "[Unnamed]" : net->GetName()) << " (ID: " << GetNetId() << ")\n";
            } else {
                oss << "Net ID: " << GetNetId() << " [Not Found]\n";
            }
        } else {
            oss << "Net ID: " << GetNetId() << "\n";
        }
    }

    oss << "Width: " << width << "\n";
    oss << "From: (" << x1 << ", " << y1 << ")\n";
    oss << "To: (" << x2 << ", " << y2 << ")";
    return oss.str();
}

void Trace::Translate(double dx, double dy)
{
    x1 += dx;
    y1 += dy;
    x2 += dx;
    y2 += dy;
    // Width is not affected by translation
}

void Trace::Mirror(double center_axis)
{
    // Mirror both endpoints around the vertical axis
    x1 = 2 * center_axis - x1;
    x2 = 2 * center_axis - x2;
    // Y coordinates and width remain unchanged
}

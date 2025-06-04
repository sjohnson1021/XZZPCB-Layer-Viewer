#include "pcb/elements/Via.hpp"

#include <algorithm>  // For std::max
#include <cmath>      // For sqrt in proper hit detection (not used in current simple version)
#include <sstream>

#include <blend2d.h>

#include "pcb/elements/Component.hpp"  // For parentComponent, though not used by Via
#include "utils/GeometryUtils.hpp"     // For geometric calculations

BLRect Via::GetBoundingBox(const Component* /*parentComponent*/) const
{
    double max_radius = std::max(pad_radius_from, pad_radius_to);
    if (max_radius == 0 && drill_diameter > 0)
        max_radius = drill_diameter / 2.0;
    else if (max_radius == 0)
        max_radius = 0.1;  // Default small radius if no info
    return BLRect(x - max_radius, y - max_radius, max_radius * 2, max_radius * 2);
}

bool Via::IsHit(const Vec2& worldMousePos, float tolerance, const Component* /*parentComponent*/) const
{
    double max_radius = std::max(pad_radius_from, pad_radius_to);
    if (max_radius == 0 && drill_diameter > 0)
        max_radius = drill_diameter / 2.0;
    else if (max_radius == 0)
        max_radius = 0.1;  // Default small radius if no info, or consider assertion/logging

    return geometry_utils::IsPointInCircle(worldMousePos,
                                           Vec2(x, y),  // Center of the Via
                                           max_radius,  // Effective radius of the Via
                                           static_cast<double>(tolerance));
}

std::string Via::GetInfo(const Component* /*parentComponent*/) const
{
    std::ostringstream oss;
    oss << "Via\n";
    oss << "Position: (" << x << ", " << y << ")\n";
    oss << "Layers: " << layer_from << " to " << layer_to << " (Primary Element Layer: " << GetLayerId() << ")\n";
    if (GetNetId() != -1) {
        oss << "Net ID: " << GetNetId() << "\n";
    }
    oss << "Drill Dia: " << drill_diameter << "\n";
    oss << "Pad From Layer: " << pad_radius_from << ", Pad To Layer: " << pad_radius_to;
    if (!optional_text.empty()) {
        oss << "\nText: " << optional_text;
    }
    return oss.str();
}

void Via::Translate(double dx, double dy)
{
    x += dx;
    y += dy;
    // Other properties (diameters, layers) are not affected by translation.
}

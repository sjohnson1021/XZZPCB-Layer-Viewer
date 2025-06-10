#include "Component.hpp"

#include <algorithm>  // For std::min, std::max
#include <cmath>      // For cos, sin, fmod
#include <sstream>    // For getInfo

#include "utils/Constants.hpp"      // For kPi
#include "utils/GeometryUtils.hpp"  // Might be useful for future, more complex component shapes
#include "utils/Vec2.hpp"           // For Vec2, though indirectly used via BLRect/BLBox

// Other includes for Component methods if necessary


void Component::Mirror(double center_axis)
{
    // Mirror the component's center position across the vertical axis
    center_x = 2 * center_axis - center_x;

    // Pin coordinates are GLOBAL, so mirror them directly using their Mirror method
    for (auto& pin : pins) {
        if (!pin) continue;
        pin->Mirror(center_axis);
    }

    // Mirror text labels in the component's local coordinate system
    for (auto& label : text_labels) {
        if (!label) continue;

        // Mirror the text label's local X coordinate
        label->coords.x_ax = -label->coords.x_ax;

        // Optionally, we could also flip text alignment or rotation here
        // depending on the requirements for text rendering after mirroring
    }

    // Mirror graphical elements (silkscreen, courtyard, etc.)
    for (auto& segment : graphical_elements) {
        // Mirror both start and end points in the component's local coordinate system
        segment.start.x_ax = -segment.start.x_ax;
        segment.end.x_ax = -segment.end.x_ax;
        // Y coordinates remain unchanged for horizontal mirroring
    }

    // Update component rotation if needed
    // For horizontal mirroring, we might need to adjust rotation
    // This depends on the specific requirements and how rotation is interpreted
    // For now, we'll leave rotation unchanged, but this could be adjusted if needed

    // Note: We don't need to update pin bounding box coordinates here
    // because they will be recalculated when needed based on the new pin positions
}

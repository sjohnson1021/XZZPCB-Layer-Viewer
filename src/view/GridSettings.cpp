#include "GridSettings.hpp"

#include <string_view>

#include <blend2d.h>

GridSettings::GridSettings()
    : m_major_line_color(BLRgba32(150, 150, 150, 100)),
      m_minor_line_color(BLRgba32(119, 119, 119, 50)),
      m_x_axis_color(BLRgba32(179, 51, 51, 230)),
      m_y_axis_color(BLRgba32(51, 179, 51, 230)),
      m_background_color(BLRgba32(0, 0, 0, 0))
{
    // All members are initialized with defaults in the header.
    // Constructor can be used for more complex setup if needed in the future.
}

std::string_view GridSettings::UnitToString()
{
    switch (GridSettings::m_unit_system) {
        case GridUnitSystem::kMetric:
            return "mm";
        case GridUnitSystem::kImperial:
            return "in";
        default:
            return "units";
    }
}

float GridSettings::GetUnitDisplayScale()
{
    // For scaling internal units to display units if needed
    // e.g., if we store in mm but want to display in cm, would return 0.1f
    // For now, return 1.0f (no scaling) - can be expanded later
    return 1.0F;
}

// Implementations for other methods if they were more complex than simple setters/getters.

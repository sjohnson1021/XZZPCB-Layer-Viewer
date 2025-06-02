#include "GridSettings.hpp"

#include <string_view>

GridSettings::GridSettings()
    : m_major_line_color(0.588F, 0.588F, 0.588F, 0.392F),
      m_minor_line_color(0.467F, 0.467F, 0.467F, 0.196F),

      m_x_axis_color(0.702F, 0.2F, 0.2F, 0.902F),
      m_y_axis_color(0.2F, 0.702F, 0.2F, 0.902F),
      m_background_color(0.0F, 0.0F, 0.0F, 0.0F)

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

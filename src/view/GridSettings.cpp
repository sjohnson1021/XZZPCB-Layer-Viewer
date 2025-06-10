#include "GridSettings.hpp"

#include <cmath>
#include <string_view>

#include <blend2d.h>
#include "core/Config.hpp"

GridSettings::GridSettings()
    : m_major_line_color(BLRgba32(150, 150, 150, 100)),
      m_minor_line_color(BLRgba32(119, 119, 119, 50)),
      m_x_axis_color(BLRgba32(179, 51, 51, 230)),
      m_y_axis_color(BLRgba32(51, 179, 51, 230)),
      m_background_color(BLRgba32(0, 0, 0, 0))
{
    // Set appropriate default spacing based on unit system
    // For Imperial: 0.25 inch = 250 mils = 2500 world units (already set in header)
    // For Metric: 5.0mm = 5.0/25.4 inches = ~0.197 inches = ~197 world units
    if (m_unit_system == GridUnitSystem::kMetric) {
        m_base_major_spacing = .250F;  // 5.0mm default for metric
    }
	if (m_unit_system == GridUnitSystem::kImperial) {
        m_base_major_spacing = 1000.0F;  // 0.25" default for imperial
    }
    // Imperial default (2500.0F for 0.25") is already set in header
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
    // GRID SCALE FIX: XZZ PCB files use thousandths of an inch (mils) as their base unit.
    // The XZZ loader divides coordinates by xyscale=10000, which converts from the raw
    // file units to world coordinates. To get back to inches from mils, we need to
    // divide by 1000 (since 1 inch = 1000 mils).
    //
    // Scale factor: 0.001 converts from mils to inches
    // This gives us the correct physical measurements in inches.
    return 0.001F;
}

float GridSettings::InchesToMm(float inches)
{
    // Standard conversion: 1 inch = 25.4 millimeters
    return inches * 25.4F;
}

float GridSettings::MmToInches(float mm)
{
    // Standard conversion: 1 inch = 25.4 millimeters
    return mm / 25.4F;
}

float GridSettings::WorldUnitsToInches(float world_units)
{
    // XZZ files store coordinates in mils, divided by xyscale=10000 to get world coordinates
    // World coordinates are in units of 0.1 mils (1/10000 inch)
    // To convert to inches: world_units * (0.1 mils) * (1 inch / 1000 mils) = world_units / 10000
    return world_units / 1000.0F;
}

float GridSettings::InchesToWorldUnits(float inches)
{
    // Convert inches to world coordinate units (0.1 mils)
    // inches * (1000 mils / inch) * (10 world_units / mil) = inches * 10000
    return inches * 1000.0F;
}

float GridSettings::WorldUnitsToMm(float world_units)
{
    // Convert world units to inches first, then to mm
    float inches = WorldUnitsToInches(world_units);
    return InchesToMm(inches);
}

float GridSettings::MmToWorldUnits(float mm)
{
    // Convert mm to inches first, then to world units
    float inches = MmToInches(mm);
    return InchesToWorldUnits(inches);
}

float GridSettings::GetCleanImperialSpacing(float inches)
{
    // Common imperial grid spacings in inches
    static const float commonImperialSpacings[] = {
        0.01F, 0.025F, 0.05F, 0.1F, 0.125F, 0.25F, 0.5F, 1.0F, 2.0F, 4.0F, 6.0F, 12.0F
    };

    // Find the closest common spacing
    float bestSpacing = commonImperialSpacings[0];
    float minDiff = std::abs(inches - bestSpacing);

    for (float spacing : commonImperialSpacings) {
        float diff = std::abs(inches - spacing);
        if (diff < minDiff) {
            minDiff = diff;
            bestSpacing = spacing;
        }
    }

    return bestSpacing;
}

float GridSettings::GetCleanMetricSpacing(float mm)
{
    // Common metric grid spacings in millimeters
    static const float commonMetricSpacings[] = {
        0.1F, 0.25F, 0.5F, 1.0F, 2.0F, 2.5F, 5.0F, 10.0F, 20.0F, 25.0F, 50.0F, 100.0F, 200.0F, 250.0F, 500.0F
    };

    // Find the closest common spacing
    float bestSpacing = commonMetricSpacings[0];
    float minDiff = std::abs(mm - bestSpacing);

    for (float spacing : commonMetricSpacings) {
        float diff = std::abs(mm - spacing);
        if (diff < minDiff) {
            minDiff = diff;
            bestSpacing = spacing;
        }
    }

    return bestSpacing;
}

void GridSettings::LoadSettingsFromConfig(const Config& config)
{
    // Load basic grid settings
    m_visible = config.GetBool("grid.visible", m_visible);
    m_is_dynamic = config.GetBool("grid.dynamic", m_is_dynamic);
    m_show_axis_lines = config.GetBool("grid.show_axis_lines", m_show_axis_lines);
    m_show_measurement_readout = config.GetBool("grid.show_measurement_readout", m_show_measurement_readout);

    // Load unit system
    int unit_system_int = config.GetInt("grid.unit_system", static_cast<int>(m_unit_system));
    m_unit_system = static_cast<GridUnitSystem>(unit_system_int);

    // Load style
    int style_int = config.GetInt("grid.style", static_cast<int>(m_style));
    m_style = static_cast<GridStyle>(style_int);

    // Load spacing and subdivisions
    m_base_major_spacing = config.GetFloat("grid.base_major_spacing", m_base_major_spacing);
    m_subdivisions = config.GetInt("grid.subdivisions", m_subdivisions);

    // Load pixel step limits
    m_min_pixel_step = config.GetFloat("grid.min_pixel_step", m_min_pixel_step);
    m_max_pixel_step = config.GetFloat("grid.max_pixel_step", m_max_pixel_step);

    // Load line/dot appearance
    m_line_thickness = config.GetFloat("grid.line_thickness", m_line_thickness);
    m_axis_line_thickness = config.GetFloat("grid.axis_line_thickness", m_axis_line_thickness);
    m_dot_radius = config.GetFloat("grid.dot_radius", m_dot_radius);

    // Load colors (stored as uint32 RGBA values)
    m_major_line_color = BLRgba32(config.GetInt("grid.major_line_color", static_cast<int>(m_major_line_color.value)));
    m_minor_line_color = BLRgba32(config.GetInt("grid.minor_line_color", static_cast<int>(m_minor_line_color.value)));
    m_x_axis_color = BLRgba32(config.GetInt("grid.x_axis_color", static_cast<int>(m_x_axis_color.value)));
    m_y_axis_color = BLRgba32(config.GetInt("grid.y_axis_color", static_cast<int>(m_y_axis_color.value)));
    m_background_color = BLRgba32(config.GetInt("grid.background_color", static_cast<int>(m_background_color.value)));
}

void GridSettings::SaveSettingsToConfig(Config& config) const
{
    // Save basic grid settings
    config.SetBool("grid.visible", m_visible);
    config.SetBool("grid.dynamic", m_is_dynamic);
    config.SetBool("grid.show_axis_lines", m_show_axis_lines);
    config.SetBool("grid.show_measurement_readout", m_show_measurement_readout);

    // Save unit system and style
    config.SetInt("grid.unit_system", static_cast<int>(m_unit_system));
    config.SetInt("grid.style", static_cast<int>(m_style));

    // Save spacing and subdivisions
    config.SetFloat("grid.base_major_spacing", m_base_major_spacing);
    config.SetInt("grid.subdivisions", m_subdivisions);

    // Save pixel step limits
    config.SetFloat("grid.min_pixel_step", m_min_pixel_step);
    config.SetFloat("grid.max_pixel_step", m_max_pixel_step);

    // Save line/dot appearance
    config.SetFloat("grid.line_thickness", m_line_thickness);
    config.SetFloat("grid.axis_line_thickness", m_axis_line_thickness);
    config.SetFloat("grid.dot_radius", m_dot_radius);

    // Save colors as uint32 RGBA values
    config.SetInt("grid.major_line_color", static_cast<int>(m_major_line_color.value));
    config.SetInt("grid.minor_line_color", static_cast<int>(m_minor_line_color.value));
    config.SetInt("grid.x_axis_color", static_cast<int>(m_x_axis_color.value));
    config.SetInt("grid.y_axis_color", static_cast<int>(m_y_axis_color.value));
    config.SetInt("grid.background_color", static_cast<int>(m_background_color.value));
}

// Implementations for other methods if they were more complex than simple setters/getters.

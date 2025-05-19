#include "view/GridSettings.hpp"

GridSettings::GridSettings()
    : m_visible(true)
    , m_style(GridStyle::LINES)
    , m_isDynamic(true)
    , m_baseMajorSpacing(100.0f)
    , m_subdivisions(10)
    , m_minPixelStep(8.0f)
    , m_maxPixelStep(1024.0f)
    , m_majorLineColor(0.588f, 0.588f, 0.588f, 0.392f)
    , m_minorLineColor(0.467f, 0.467f, 0.467f, 0.196f)
    , m_showAxisLines(true)
    , m_xAxisColor(0.702f, 0.2f, 0.2f, 0.902f)
    , m_yAxisColor(0.2f, 0.702f, 0.2f, 0.902f)
    , m_backgroundColor(0.0f, 0.0f, 0.0f, 0.0f)
    , m_lineThickness(1.0f)
    , m_axisLineThickness(1.0f)
    , m_dotRadius(1.0f)
    , m_showMeasurementReadout(true)
{
    // All members are initialized with defaults in the header.
    // Constructor can be used for more complex setup if needed in the future.
}

const char* GridSettings::UnitToString() const {
    switch (m_unitSystem) {
        case GridUnitSystem::METRIC:
            return "mm";
        case GridUnitSystem::IMPERIAL:
            return "in";
        default:
            return "units";
    }
}

float GridSettings::GetUnitDisplayScale() const {
    // For scaling internal units to display units if needed
    // e.g., if we store in mm but want to display in cm, would return 0.1f
    // For now, return 1.0f (no scaling) - can be expanded later
    return 1.0f;
}

// Implementations for other methods if they were more complex than simple setters/getters. 
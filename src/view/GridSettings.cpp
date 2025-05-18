#include "view/GridSettings.hpp"

GridSettings::GridSettings()
    : m_visible(true)
    , m_style(GridStyle::LINES)
    , m_isDynamic(true)
    , m_baseMajorSpacing(50)
    , m_subdivisions(10)
    , m_minPixelStep(32.0f)
    , m_maxPixelStep(128.0f)
{
    // All members are initialized with defaults in the header.
    // Constructor can be used for more complex setup if needed in the future.
}

// Implementations for other methods if they were more complex than simple setters/getters. 
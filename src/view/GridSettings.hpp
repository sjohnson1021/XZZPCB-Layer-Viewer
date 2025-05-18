#pragma once

// #include "utils/Color.hpp" // If we have a dedicated Color struct/class
// For now, let's assume simple color representation or define one.
struct GridColor {
    float r = 0.5f;
    float g = 0.5f;
    float b = 0.5f;
    float a = 1.0f;

    GridColor() = default;
    GridColor(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
};

enum class GridStyle {
    LINES,
    DOTS
};

enum class GridUnitSystem {
    METRIC,  // e.g., mm, cm
    IMPERIAL // e.g., inches, mils
};

class GridSettings {
public:
    GridSettings();

    // Keep getters if some logic is involved or for read-only external access
    // but for ImGui, direct member access is simpler if we make them public.

//private: // Original access specifier
public:  // Allow direct access for ImGui widgets
    bool m_visible = true;
    GridStyle m_style = GridStyle::LINES;
    GridUnitSystem m_unitSystem = GridUnitSystem::METRIC;

    // Base spacing for major grid lines (e.g., in millimeters or other world units)
    // This is the spacing when zoom is 1.0 and dynamic adjustment hasn't kicked in significantly.
    int m_baseMajorSpacing = 50; // Changed from float to int

    // Number of subdivisions between major grid lines to draw minor lines
    int m_subdivisions = 10;

    GridColor m_majorLineColor = {0.3f, 0.3f, 0.3f, 1.0f};
    GridColor m_minorLineColor = {0.15f, 0.15f, 0.15f, 1.0f};

    bool m_isDynamic = true; 
    float m_minPixelStep = 20.0f; // Default for dynamic grid spacing
    float m_maxPixelStep = 80.0f; // Default for dynamic grid spacing

    bool m_showAxisLines = true;
    GridColor m_xAxisColor = {0.7f, 0.2f, 0.2f, 0.9f};
    GridColor m_yAxisColor = {0.2f, 0.7f, 0.2f, 0.9f};

    GridColor m_backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f}; // Grid's own background color

    // New members for line/dot thickness
    float m_lineThickness = 1.0f;
    float m_axisLineThickness = 1.0f; // Can be different from regular lines
    float m_dotRadius = 1.0f;         // Radius for dots in DOTS style

    // Original public methods - can be kept or removed if direct access is preferred
    // bool IsVisible() const { return m_visible; }
    // void SetVisible(bool visible) { m_visible = visible; }
    // ... other getters/setters ...

    // To still allow controlled modification or read-only access, keep selected getters/setters
    // For simplicity with ImGui, the members are now public.
    // We can refine this later if stricter encapsulation is needed for non-ImGui interactions.
}; 
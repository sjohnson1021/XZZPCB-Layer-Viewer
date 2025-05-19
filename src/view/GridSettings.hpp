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

    // Required helper methods for unit conversion and display
    const char* UnitToString() const;
    float GetUnitDisplayScale() const; // For converting internal units to display units

    // Keep getters if some logic is involved or for read-only external access
    // but for ImGui, direct member access is simpler if we make them public.

//private: // Original access specifier
public:  // Allow direct access for ImGui widgets
    bool m_visible = true;
    GridStyle m_style = GridStyle::LINES;
    GridUnitSystem m_unitSystem = GridUnitSystem::METRIC; // Hide from UI for now

    // Base spacing for major grid lines in world units (mm for metric, inches for imperial)
    // This is the spacing when zoom is 1.0 and dynamic adjustment hasn't kicked in significantly.
    float m_baseMajorSpacing = 100.0f;

    // Number of subdivisions between major grid lines to draw minor lines
    int m_subdivisions = 10;

    GridColor m_majorLineColor = {0.588f, 0.588f, 0.588f, 0.392f}; // 150,150,150,100
    GridColor m_minorLineColor = {0.467f, 0.467f, 0.467f, 0.196f}; // 119,119,119,50

    bool m_isDynamic = true; 
    float m_minPixelStep = 8.0f;
    float m_maxPixelStep = 1024.0f;

    bool m_showAxisLines = true;
    GridColor m_xAxisColor = {0.702f, 0.2f, 0.2f, 0.902f}; // 179,51,51,230
    GridColor m_yAxisColor = {0.2f, 0.702f, 0.2f, 0.902f}; // 51,179,51,230

    GridColor m_backgroundColor = {0.0f, 0.0f, 0.0f, 0.0f}; // Grid's own background color

    // Line/dot thickness/radius settings
    float m_lineThickness = 1.0f;
    float m_axisLineThickness = 1.0f; // Can be different from regular lines
    float m_dotRadius = 1.0f;         // Radius for dots in DOTS style

    // New: Show measurement readout on screen
    bool m_showMeasurementReadout = true;
    
    // Safety limits to prevent excessive rendering
    static constexpr int MAX_RENDERABLE_LINES = 5000; // Hard limit on total lines to render
    static constexpr int MAX_RENDERABLE_DOTS = 10000; // Hard limit on total dots to render

    // Original public methods - can be kept or removed if direct access is preferred
    // bool IsVisible() const { return m_visible; }
    // void SetVisible(bool visible) { m_visible = visible; }
    // ... other getters/setters ...

    // To still allow controlled modification or read-only access, keep selected getters/setters
    // For simplicity with ImGui, the members are now public.
    // We can refine this later if stricter encapsulation is needed for non-ImGui interactions.
}; 
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

    float m_baseMajorSpacing = 10.0f; 
    int m_subdivisions = 10;        

    GridColor m_majorLineColor = {0.3f, 0.3f, 0.3f, 1.0f};
    GridColor m_minorLineColor = {0.15f, 0.15f, 0.15f, 1.0f};

    bool m_isDynamic = true; 

    bool m_showAxisLines = true;
    GridColor m_xAxisColor = {0.7f, 0.2f, 0.2f, 0.9f};
    GridColor m_yAxisColor = {0.2f, 0.7f, 0.2f, 0.9f};

    // Original public methods - can be kept or removed if direct access is preferred
    // bool IsVisible() const { return m_visible; }
    // void SetVisible(bool visible) { m_visible = visible; }
    // ... other getters/setters ...

    // To still allow controlled modification or read-only access, keep selected getters/setters
    // For simplicity with ImGui, the members are now public.
    // We can refine this later if stricter encapsulation is needed for non-ImGui interactions.
}; 
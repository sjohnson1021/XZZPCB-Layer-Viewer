#pragma once
#include <string_view>

#include <blend2d.h>

#include "Grid.hpp"

// #include "utils/Color.hpp" // If we have a dedicated Color struct/class
// For now, let's assume simple color representation or define one.

enum class GridStyle : uint8_t {
    kLines,
    kDots
};

enum class GridUnitSystem : uint8_t {
    kMetric,   // e.g., mm, cm
    kImperial  // e.g., inches, mils
};

class GridSettings
{
public:
    GridSettings();

    // Required helper methods for unit conversion and display
    [[nodiscard]] std::string_view UnitToString();
    static float GetUnitDisplayScale();  // For converting internal units to display units
    static float InchesToMm(float inches);  // Convert inches to millimeters
    static float MmToInches(float mm);     // Convert millimeters to inches
    static float GetCleanImperialSpacing(float inches);  // Get clean imperial spacing value
    static float GetCleanMetricSpacing(float mm);        // Get clean metric spacing value

    // Configuration persistence methods
    void LoadSettingsFromConfig(const class Config& config);
    void SaveSettingsToConfig(class Config& config) const;

    // World coordinate conversion helpers
    static float WorldUnitsToInches(float world_units);   // Convert world coordinates to inches
    static float InchesToWorldUnits(float inches);        // Convert inches to world coordinates
    static float WorldUnitsToMm(float world_units);       // Convert world coordinates to mm
    static float MmToWorldUnits(float mm);                // Convert mm to world coordinates

    // Keep getters if some logic is involved or for read-only external access
    // but for ImGui, direct member access is simpler if we make them public.

    // private: // Original access specifier
    //  Allow direct access for ImGui widgets
    bool m_visible = true;
    GridStyle m_style = GridStyle::kLines;
    GridUnitSystem m_unit_system = GridUnitSystem::kImperial;  // Default to Imperial since XZZ files use mils/inches

    // Base spacing for major grid lines in world coordinate units (0.1 mils)
    // This is the spacing when zoom is 1.0 and dynamic adjustment hasn't kicked in significantly.
    // Default value of 2500.0 represents 0.25 inch (2500 * 0.1 mils = 250 mils = 0.25 inch)
    // This provides better default spacing for PCB design work
    float m_base_major_spacing = 2500.0F;

    // Number of subdivisions between major grid lines to draw minor lines
    int m_subdivisions = 10;

    BLRgba32 m_major_line_color = {BLRgba32(150, 150, 150, 100)};  // 150,150,150,100
    BLRgba32 m_minor_line_color = {BLRgba32(119, 119, 119, 50)};   // 119,119,119,50

    bool m_is_dynamic = true;
    float m_min_pixel_step = 8.0F;
    float m_max_pixel_step = 1024.0F;

    bool m_show_axis_lines = true;
    BLRgba32 m_x_axis_color = {BLRgba32(179, 51, 51, 230)};  // 179,51,51,230
    BLRgba32 m_y_axis_color = {BLRgba32(51, 179, 51, 230)};  // 51,179,51,230

    BLRgba32 m_background_color = {BLRgba32(0, 0, 0, 0)};  // Grid's own background color

    // Line/dot thickness/radius settings
    float m_line_thickness = 1.0F;
    float m_axis_line_thickness = 1.0F;  // Can be different from regular lines
    float m_dot_radius = 1.0F;           // Radius for dots in DOTS style

    // New: Show measurement readout on screen
    bool m_show_measurement_readout = true;

    // Safety limits to prevent excessive rendering
    static constexpr int kMaxRenderableLines = 5000;  // Hard limit on total lines to render
    static constexpr int kMaxRenderableDots = 7500;   // Hard limit on total dots to render

    // Original public methods - can be kept or removed if direct access is preferred
    // bool IsVisible() const { return m_visible; }
    // void SetVisible(bool visible) { m_visible = visible; }
    // ... other getters/setters ...

    // To still allow controlled modification or read-only access, keep selected getters/setters
    // For simplicity with ImGui, the members are now public.
    // We can refine this later if stricter encapsulation is needed for non-ImGui interactions.
};

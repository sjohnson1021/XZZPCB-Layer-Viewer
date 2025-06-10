#pragma once

#include <memory>
#include <string>
#include "imgui.h"
#include "../../view/GridSettings.hpp"
#include "../../core/ControlSettings.hpp"
#include "../../core/BoardDataManager.hpp"

// Forward declarations
// GridSettings, ControlSettings, and BoardDataManager are already included above
// Potentially other settings classes in the future
// class ThemeSettings;
// class ApplicationSettings;
class Board; // Forward declare Board
class Grid;  // Forward declare Grid for font invalidation

class SettingsWindow {
public:
    // It will need access to the settings objects it can modify.
    SettingsWindow(std::shared_ptr<GridSettings> grid_settings,
                   std::shared_ptr<ControlSettings> control_settings,
                   std::shared_ptr<BoardDataManager> board_data_manager,
                   float* application_clear_color,  // Added pointer to application's clear color
                   std::shared_ptr<Grid> grid = nullptr  // Added Grid for font invalidation
    );
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;
    SettingsWindow(SettingsWindow&&) = delete;
    SettingsWindow& operator=(SettingsWindow&&) = delete;

    void RenderUI(const std::shared_ptr<Board>& current_board);

    [[nodiscard]] bool IsWindowVisible() const { return m_is_open_; }
    void SetVisible(bool visible) { m_is_open_ = visible; }

    void ToggleVisibility() { m_is_open_ = !m_is_open_; }
    [[nodiscard]] bool IsOpen() const { return m_is_open_; }

    // Font settings persistence (public for Application access)
    void LoadFontSettingsFromConfig(const class Config& config);
    void SaveFontSettingsToConfig(class Config& config) const;

private:
    std::string m_window_name = "Settings";
    bool m_is_open_ = false;  // Default to closed, opened via a menu typically

    std::shared_ptr<GridSettings> m_grid_settings_;
    std::shared_ptr<ControlSettings> m_control_settings_;
    float* m_app_clear_color_;  // Pointer to Application's clear color
    std::shared_ptr<BoardDataManager> m_board_data_manager_;
    std::shared_ptr<Grid> m_grid_;  // Grid instance for font invalidation
    // std::shared_ptr<ThemeSettings> m_themeSettings;
    // std::shared_ptr<ApplicationSettings> m_appSettings;

    // Font accessibility settings
    float m_font_scale_multiplier_ = 1.0f;  // Base font scale multiplier (0.5x to 3.0x)
    bool m_font_settings_changed_ = false;  // Track if font settings need to be applied

    // Helper methods for different settings sections
    void ShowGridSettings();
    // void ShowThemeSettings();
    // void ShowApplicationSettings();
    void ShowControlSettings();
    void ShowAppearanceSettings(const std::shared_ptr<Board>& current_board);
    void ShowLayerControls(const std::shared_ptr<Board>& current_board);
    void ShowAccessibilitySettings();

    // Helper method for rendering color controls
    void RenderColorControl(const char* label, BoardDataManager::ColorType color_type, const char* tooltip);

    // Helper method to trigger grid redraw when grid settings change
    void TriggerGridRedraw();
};

// Factory function to create SettingsWindow - helps with linking issues
std::unique_ptr<SettingsWindow> CreateSettingsWindow(
    std::shared_ptr<GridSettings> grid_settings,
    std::shared_ptr<ControlSettings> control_settings,
    std::shared_ptr<BoardDataManager> board_data_manager,
    float* application_clear_color,
    std::shared_ptr<Grid> grid = nullptr
);
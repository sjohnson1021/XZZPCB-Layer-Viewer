#pragma once

#include <memory>
#include <string>
#include "imgui.h"
#include "../../view/GridSettings.hpp"
#include "../../core/ControlSettings.hpp"
#include "../../core/BoardDataManager.hpp"

// Forward declarations
class GridSettings;
// Potentially other settings classes in the future
// class ThemeSettings;
// class ApplicationSettings;
class ControlSettings;
class Board; // Forward declare Board

class SettingsWindow {
public:
    // It will need access to the settings objects it can modify.
    SettingsWindow(std::shared_ptr<GridSettings> grid_settings,
                   std::shared_ptr<ControlSettings> control_settings,
                   std::shared_ptr<BoardDataManager> board_data_manager,
                   float* application_clear_color  // Added pointer to application's clear color
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

private:
    std::string m_window_name = "Settings";
    bool m_is_open_ = false;  // Default to closed, opened via a menu typically

    std::shared_ptr<GridSettings> m_grid_settings_;
    std::shared_ptr<ControlSettings> m_control_settings_;
    float* m_app_clear_color_;  // Pointer to Application's clear color
    std::shared_ptr<BoardDataManager> m_board_data_manager_;
    // std::shared_ptr<ThemeSettings> m_themeSettings;
    // std::shared_ptr<ApplicationSettings> m_appSettings;

    // Helper methods for different settings sections
    void ShowGridSettings();
    // void ShowThemeSettings();
    // void ShowApplicationSettings();
    void ShowControlSettings();
    void ShowAppearanceSettings(const std::shared_ptr<Board>& current_board);
    void ShowLayerControls(const std::shared_ptr<Board>& current_board);
}; 
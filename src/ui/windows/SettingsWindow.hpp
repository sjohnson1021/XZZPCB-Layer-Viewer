#pragma once

#include <memory>
#include <string>
#include "imgui.h"

// Forward declarations
class GridSettings;
// Potentially other settings classes in the future
// class ThemeSettings;
// class ApplicationSettings;

class SettingsWindow {
public:
    // It will need access to the settings objects it can modify.
    explicit SettingsWindow(std::shared_ptr<GridSettings> gridSettings /*, other settings */);
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;
    SettingsWindow(SettingsWindow&&) = delete;
    SettingsWindow& operator=(SettingsWindow&&) = delete;

    void RenderUI();

    bool IsWindowVisible() const { return m_isOpen; }
    void SetVisible(bool visible) { m_isOpen = visible; }

private:
    std::string m_windowName = "Settings";
    bool m_isOpen = false; // Default to closed, opened via a menu typically

    std::shared_ptr<GridSettings> m_gridSettings;
    // std::shared_ptr<ThemeSettings> m_themeSettings;
    // std::shared_ptr<ApplicationSettings> m_appSettings;

    // Helper methods for different settings sections
    void ShowGridSettings();
    // void ShowThemeSettings();
    // void ShowApplicationSettings();
}; 
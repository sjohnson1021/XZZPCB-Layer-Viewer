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
    SettingsWindow(
        std::shared_ptr<GridSettings> gridSettings,
        std::shared_ptr<ControlSettings> controlSettings,
        std::shared_ptr<BoardDataManager> boardDataManager,
        float* applicationClearColor // Added pointer to application's clear color
    );
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;
    SettingsWindow(SettingsWindow&&) = delete;
    SettingsWindow& operator=(SettingsWindow&&) = delete;

    void RenderUI(const std::shared_ptr<Board>& currentBoard);

    bool IsWindowVisible() const { return m_isOpen; }
    void SetVisible(bool visible) { m_isOpen = visible; }

    void ToggleVisibility() { m_isOpen = !m_isOpen; }
    bool IsOpen() const { return m_isOpen; }

private:
    std::string m_windowName = "Settings";
    bool m_isOpen = false; // Default to closed, opened via a menu typically

    std::shared_ptr<GridSettings> m_gridSettings;
    std::shared_ptr<ControlSettings> m_controlSettings;
    float* m_appClearColor; // Pointer to Application's clear color
    std::shared_ptr<BoardDataManager> m_boardDataManager;
    // std::shared_ptr<ThemeSettings> m_themeSettings;
    // std::shared_ptr<ApplicationSettings> m_appSettings;

    // Helper methods for different settings sections
    void ShowGridSettings();
    // void ShowThemeSettings();
    // void ShowApplicationSettings();
    void ShowControlSettings();
    void ShowAppearanceSettings(const std::shared_ptr<Board>& currentBoard);
    void ShowLayerControls(const std::shared_ptr<Board>& currentBoard);
}; 
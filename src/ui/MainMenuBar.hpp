#pragma once

#include <functional>

// Forward declaration
class Application;

class MainMenuBar {
public:
    MainMenuBar();
    ~MainMenuBar();

    void RenderUI(Application& app);

    // Flags for menu actions to be checked by Application
    bool WantsToOpenFile() { bool current = m_wantsToOpenFile; m_wantsToOpenFile = false; return current; }
    bool WantsToExit() { bool current = m_wantsToExit; m_wantsToExit = false; return current; }
    
    // Toggle states for menu items
    bool* GetShowDemoWindowFlag() { return &m_showDemoWindow; }
    void SetSettingsWindowVisible(bool isVisible) { m_isSettingsWindowVisible = isVisible; }
    bool WantsToToggleSettings() { bool current = m_wantsToToggleSettings; m_wantsToToggleSettings = false; return current; }


private:
    bool m_showDemoWindow;
    bool m_isSettingsWindowVisible; // Controlled by Application, set here for checkmark state

    // Action flags, to be reset after check
    bool m_wantsToOpenFile;
    bool m_wantsToExit;
    bool m_wantsToToggleSettings;

    // State for ImGui helper windows, as Application.hpp indicated MainMenuBar would handle these
    bool m_showImGuiDemoWindow = false;
    bool m_showImGuiMetricsWindow = false; 
}; 
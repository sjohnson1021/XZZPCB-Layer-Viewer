#pragma once

#include <string>
#include <memory>
// #include <imgui.h> // Forward declare ImVec2 if needed, or include if Application directly uses ImGui types beyond ImVec2
// #include <core/ControlSettings.hpp> // Included by PcbViewerWindow and SettingsWindow

// Forward declarations for core systems
class Config;
class Events;
class Renderer;
class ImGuiManager;
class ControlSettings; // Now a member

// Forward declarations for UI classes
class MainMenuBar;
class PCBViewerWindow;
class SettingsWindow;
class PcbDetailsWindow;

// Forward declarations for view and PCB data classes
class Camera;
class Viewport;
class Grid;
class GridSettings;
class Board; // For currentBoard

// For ImGuiFileDialog
#include "ImGuiFileDialog.h" // Better to forward declare if only pointer/reference is stored


class Application {
public:
    Application();
    ~Application();

    // Main lifecycle methods
    bool Initialize(); // Removed argc, argv as they are not used
    int Run();
    void Shutdown();
    
    // State and control
    bool IsRunning() const;
    void Quit();

    // Accessors for core subsystems (consider if all are needed publicly)
    Config* GetConfig() const;
    Events* GetEvents() const;
    Renderer* GetRenderer() const;
    ImGuiManager* GetImGuiManager() const;
    // ControlSettings* GetControlSettings() const; // If needed by UI outside constructor

    // Menu action request setters
    void SetOpenFileRequested(bool requested) { m_openFileRequested = requested; }
    void SetQuitFileRequested(bool requested) { m_quitFileRequested = requested; }
    void SetShowSettingsRequested(bool requested) { m_showSettingsRequested = requested; }
    void SetShowPcbDetailsRequested(bool requested) { m_showPcbDetailsRequested = requested; }

private:
    // Initialization helpers
    bool InitializeCoreSubsystems();
    bool InitializeUISubsystems();
    void LoadConfig();
    void SetupDefaultKeybinds(); // If ControlSettings loaded from config fails or is partial

    // Main loop phases
    void ProcessEvents();
    void Update(float deltaTime); // Add deltaTime if Application has timed updates
    void Render();

    // UI Rendering Helper
    void RenderUI(); // New method to group UI rendering calls

    // PCB File Handling
    void OpenPcbFile(const std::string& filePath);

    // Core Subsystems
    std::unique_ptr<Config> m_config;
    std::unique_ptr<Events> m_events;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<ImGuiManager> m_imguiManager;
    
    // Application State
    bool m_isRunning;
    std::string m_appName;
    int m_windowWidth;
    int m_windowHeight;
    float m_clearColor[4]; // For background of SDL window

    // View and PCB Data (to be passed to UI windows)
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<Viewport> m_viewport;
    std::shared_ptr<GridSettings> m_gridSettings;
    std::shared_ptr<ControlSettings> m_controlSettings;
    std::shared_ptr<Grid> m_grid;
    std::shared_ptr<Board> m_currentBoard;

    // UI Window Instances
    std::unique_ptr<MainMenuBar> m_mainMenuBar;
    std::unique_ptr<PCBViewerWindow> m_pcbViewerWindow;
    std::unique_ptr<SettingsWindow> m_settingsWindow;
    std::unique_ptr<PcbDetailsWindow> m_pcbDetailsWindow;
    // No longer directly in Application: bool m_showDemoWindow; (handled by MainMenuBar)
    // No longer directly in Application: bool m_showLayerControls; (logic to be moved to SettingsWindow or similar)
    // No longer directly in Application: ImVec2 m_contentAreaPos; ImVec2 m_contentAreaSize; (handled by PCBViewerWindow)

    // File Dialog
    // std::unique_ptr<ImGuiFileDialog::FileDialog> m_fileDialogInstance; // CHANGE THIS
    std::unique_ptr<ImGuiFileDialog> m_fileDialogInstance; // TO THIS - If ImGuiFileDialog is the direct type name

    // UI State for Modals
    bool m_showPcbLoadErrorModal;
    std::string m_pcbLoadErrorMessage;

    // Menu action request flags
    bool m_openFileRequested = false;
    bool m_quitFileRequested = false;
    bool m_showSettingsRequested = false;
    bool m_showPcbDetailsRequested = false;
};
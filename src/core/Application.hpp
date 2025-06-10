#pragma once

#include <string>
#include <memory>
#include "../../external/ImGuiFileDialog/ImGuiFileDialog.h" // Better to forward declare if only pointer/reference is stored
// #include <imgui.h> // Forward declare ImVec2 if needed, or include if Application directly uses ImGui types beyond ImVec2
// #include <core/ControlSettings.hpp> // Included by PcbViewerWindow and SettingsWindow

// Forward declarations for core systems
class Config;
class Events;
class Renderer;
class ImGuiManager;
enum class WindowEventType; // Forward declare for window event handling
class ControlSettings; // Now a member
class PcbRenderer;     // Forward declare PcbRenderer
// class PcbLoader; // No longer needed here
class BoardLoaderFactory; // Forward declare BoardLoaderFactory

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
class Board;            // For currentBoard
class BoardDataManager; // Forward declare BoardDataManager

// For ImGuiFileDialog

class Application
{
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
    Config *GetConfig() const;
    Events *GetEvents() const;
    Renderer *GetRenderer() const;
    ImGuiManager *GetImGuiManager() const;
    // ControlSettings* GetControlSettings() const; // If needed by UI outside constructor

    // Menu action request setters
    void SetQuitFileRequested(bool requested) { m_quitFileRequested = requested; }
    void SetShowSettingsRequested(bool requested) { m_showSettingsRequested = requested; }
    void SetShowPcbDetailsRequested(bool requested) { m_showPcbDetailsRequested = requested; }
    void SetShowFileDialogWindow(bool show) { m_showFileDialogWindow = show; }

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
    void ProcessGlobalKeyboardShortcuts(); // Process global keyboard shortcuts

    // File Dialog Management
    void InitializeFileDialogPlaces();
    void LoadFileDialogBookmarks();
    void SaveFileDialogBookmarks();
    void ClampFileDialogSize();
    void RenderFileDialog();

    // PCB File Handling
    void OpenPcbFile(const std::string &filePath);

    // Window Event Handling
    void HandleWindowEvent(WindowEventType event_type);

    // Core Subsystems
    std::unique_ptr<Config> m_config;
    std::unique_ptr<Events> m_events;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<ImGuiManager> m_imguiManager;
    std::unique_ptr<PcbRenderer> m_pcbRenderer; // For Blend2D PCB rendering

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
    std::shared_ptr<BoardDataManager> m_boardDataManager; // Changed to shared_ptr

    // UI Window Instances
    std::unique_ptr<MainMenuBar> m_mainMenuBar;
    std::unique_ptr<PCBViewerWindow> m_pcbViewerWindow;
    std::unique_ptr<SettingsWindow> m_settingsWindow;
    std::unique_ptr<PcbDetailsWindow> m_pcbDetailsWindow;
    // No longer directly in Application: bool m_showDemoWindow; (handled by MainMenuBar)
    // No longer directly in Application: bool m_showLayerControls; (logic to be moved to SettingsWindow or similar)
    // No longer directly in Application: ImVec2 m_contentAreaPos; ImVec2 m_contentAreaSize; (handled by PCBViewerWindow)

    // File Dialog
    std::unique_ptr<ImGuiFileDialog> m_fileDialogInstance; // Dockable file dialog

    // UI State for Modals
    bool m_showPcbLoadErrorModal;
    std::string m_pcbLoadErrorMessage;

    // PCB Loader Factory
    std::unique_ptr<BoardLoaderFactory> m_boardLoaderFactory;

    // Menu action request flags
    bool m_quitFileRequested = false;
    bool m_showSettingsRequested = false;
    bool m_showPcbDetailsRequested = false;
    bool m_showFileDialogWindow = false;
};
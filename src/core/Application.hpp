#pragma once

#include <string>
#include <memory>
#include <imgui.h>

class Config;
class Events;
class Renderer;
class ImGuiManager;

class Application {
public:
    Application();
    ~Application();

    bool Initialize(int argc, char* argv[]);
    int Run();
    void Shutdown();
    
    bool IsRunning() const;
    void Quit();

    // Accessors for subsystems
    Config* GetConfig() const;
    Events* GetEvents() const;
    Renderer* GetRenderer() const;
    ImGuiManager* GetImGuiManager() const;

private:
    bool InitializeSubsystems();
    void ProcessEvents();
    void Update();
    void Render();

    std::unique_ptr<Config> m_config;
    std::unique_ptr<Events> m_events;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<ImGuiManager> m_imguiManager;

    bool m_isRunning;
    std::string m_appName;
    int m_windowWidth;
    int m_windowHeight;
    
    // UI state
    bool m_showDemoWindow;
    bool m_showLayerControls;
    float m_clearColor[4];
    
    // Content area
    ImVec2 m_contentAreaPos;
    ImVec2 m_contentAreaSize;
};
#pragma once

#include <SDL3/SDL_events.h>

class Renderer;

class ImGuiManager
{
public:
    ImGuiManager(Renderer* renderer);
    ~ImGuiManager();

    bool Initialize();
    void Shutdown();
    void ProcessEvent(SDL_Event* event);
    void NewFrame();
    void FinalizeImGuiDrawLists();
    void PresentImGuiDrawData();

    // Renderer recreation handling
    void OnRendererRecreated();
    bool IsValid() const;

    // UI components
    void ShowMainToolbar();
    void ShowConfigWindow(bool* p_open = nullptr);

private:
    void LoadFonts();

    Renderer* m_renderer_ {};
    bool m_initialized_ {};
    bool m_show_config_window_ {};
};

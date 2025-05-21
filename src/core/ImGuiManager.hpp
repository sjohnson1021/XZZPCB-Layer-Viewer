#pragma once

#include <SDL3/SDL.h>

class Renderer;

class ImGuiManager {
public:
    ImGuiManager(Renderer* renderer);
    ~ImGuiManager();

    bool Initialize();
    void Shutdown();
    void ProcessEvent(SDL_Event* event);
    void NewFrame();
    void FinalizeImGuiDrawLists();
    void PresentImGuiDrawData();

    // UI components
    void ShowMainToolbar();
    void ShowConfigWindow(bool* p_open = nullptr);

private:
    Renderer* m_renderer;
    bool m_initialized;
    bool m_showConfigWindow;
}; 
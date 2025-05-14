#pragma once

#include <SDL3/SDL.h>

class ImGuiManager;

class Events {
public:
    Events();
    ~Events();

    void ProcessEvents();
    bool ShouldQuit() const;
    void SetImGuiManager(ImGuiManager* imgui);

private:
    bool m_shouldQuit;
    ImGuiManager* m_imguiManager;
}; 
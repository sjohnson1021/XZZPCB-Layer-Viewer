#pragma once

#include <SDL3/SDL.h>

class ImGuiManager;

class Events
{
public:
    Events();
    ~Events();

    void ProcessEvents();
    bool ShouldQuit() const;
    void SetImGuiManager(ImGuiManager* imgui_manager);

private:
    bool m_should_quit_;
    ImGuiManager* m_imgui_manager_;
};

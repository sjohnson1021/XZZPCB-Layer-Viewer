#include "Events.hpp"
#include "ImGuiManager.hpp"
#include <imgui.h>
#include <iostream>

Events::Events()
    : m_shouldQuit(false)
    , m_imguiManager(nullptr)
{
}

Events::~Events()
{
}

void Events::ProcessEvents()
{
    SDL_Event event;
    
    // Process all queued events at once
    while (SDL_PollEvent(&event)) {
        // Let ImGui process events first
        if (m_imguiManager) {
            m_imguiManager->ProcessEvent(&event);
        }
        
        // Check if ImGui wants to capture this input
        ImGuiIO& io = ImGui::GetIO();
        bool processed = false;
        
        if (event.type == SDL_EVENT_MOUSE_MOTION ||
            event.type == SDL_EVENT_MOUSE_WHEEL ||
            event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
            event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (io.WantCaptureMouse) {
                processed = true;
            }
        }
        
        if (event.type == SDL_EVENT_KEY_DOWN ||
            event.type == SDL_EVENT_KEY_UP ||
            event.type == SDL_EVENT_TEXT_INPUT) {
            if (io.WantCaptureKeyboard) {
                processed = true;
            }
        }
        
        // Process application-specific events if not captured by ImGui
        if (!processed) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    m_shouldQuit = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) {
                        m_shouldQuit = true;
                    }
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    m_shouldQuit = true;
                    break;
            }
        }
    }
}

bool Events::ShouldQuit() const
{
    return m_shouldQuit;
}

void Events::SetImGuiManager(ImGuiManager* imgui)
{
    m_imguiManager = imgui;
} 
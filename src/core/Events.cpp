#include "Events.hpp"

#include <imgui.h>
#include <iostream>

#include "ImGuiManager.hpp"

Events::Events() : m_should_quit_(false), m_imgui_manager_(nullptr) {}

Events::~Events() {}

void Events::ProcessEvents()
{
    SDL_Event event;

    // Process all queued events at once
    while (SDL_PollEvent(&event)) {
        // Let ImGui process events first
        if (m_imgui_manager_) {
            m_imgui_manager_->ProcessEvent(&event);
        }

        // Check if ImGui wants to capture this input
        ImGuiIO& io = ImGui::GetIO();
        bool processed = false;

        if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_WHEEL || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (io.WantCaptureMouse) {
                processed = true;
            }
        }

        if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_TEXT_INPUT) {
            if (io.WantCaptureKeyboard) {
                processed = true;
            }
        }

        // Process application-specific events if not captured by ImGui
        if (!processed) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    m_should_quit_ = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) {
                        m_should_quit_ = true;
                    }
                    break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    m_should_quit_ = true;
                    break;
            }
        }
    }
}

bool Events::ShouldQuit() const
{
    return m_should_quit_;
}

void Events::SetImGuiManager(ImGuiManager* imgui_manager)
{
    m_imgui_manager_ = imgui_manager;
}

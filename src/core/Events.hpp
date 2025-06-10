#pragma once

#include <SDL3/SDL.h>
#include <functional>

class ImGuiManager;

// Window event types for handling minimization/restoration
enum class WindowEventType {
    kMinimized,
    kRestored,
    kShown,
    kHidden
};

class Events
{
public:
    Events();
    ~Events();

    void ProcessEvents();
    bool ShouldQuit() const;
    void SetImGuiManager(ImGuiManager* imgui_manager);

    // Set callback for window events (minimization, restoration, etc.)
    void SetWindowEventCallback(std::function<void(WindowEventType)> callback);

private:
    bool m_should_quit_;
    ImGuiManager* m_imgui_manager_;
    std::function<void(WindowEventType)> m_window_event_callback_;
};

#pragma once


// Forward declaration
class Application;

class MainMenuBar
{
public:
    MainMenuBar();
    ~MainMenuBar();

    void RenderUI(Application& app);

    // Flags for menu actions to be checked by Application
    bool WantsToOpenFile()
    {
        bool const kCurrent = m_wants_to_open_file_;
        m_wants_to_open_file_ = false;
        return kCurrent;
    }
    bool WantsToExit()
    {
        bool const kCurrent = m_wants_to_exit_;
        m_wants_to_exit_ = false;
        return kCurrent;
    }

    // Toggle states for menu items
    bool* GetShowDemoWindowFlag() { return &m_show_demo_window_; }
    void SetSettingsWindowVisible(bool is_visible) { m_is_settings_window_visible_ = is_visible; }
    bool WantsToToggleSettings()
    {
        bool const kCurrent = m_wants_to_toggle_settings_;
        m_wants_to_toggle_settings_ = false;
        return kCurrent;
    }


private:
    bool m_show_demo_window_ {};
    bool m_is_settings_window_visible_ {};  // Controlled by Application, set here for checkmark state

    // Action flags, to be reset after check
    bool m_wants_to_open_file_ {};
    bool m_wants_to_exit_ {};
    bool m_wants_to_toggle_settings_ {};

    // State for ImGui helper windows, as Application.hpp indicated MainMenuBar would handle these
    bool m_show_im_gui_demo_window_ = false;
    bool m_show_im_gui_metrics_window_ = false;
};

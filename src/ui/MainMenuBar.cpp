#include "MainMenuBar.hpp"

#include "imgui.h"

#include "core/Application.hpp"  // For Application& app and its methods/flags

MainMenuBar::MainMenuBar()
{
    // Constructor: Initialize any default states if necessary
    m_show_im_gui_demo_window_ = false;
    m_show_im_gui_metrics_window_ = false;
}

MainMenuBar::~MainMenuBar()
{
    // Destructor: Cleanup if necessary
}

void MainMenuBar::RenderUI(Application& app)
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open PCB File...", "Ctrl+O")) {
                app.SetShowFileDialogWindow(true);  // Show dockable file dialog
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                app.SetQuitFileRequested(true);  // Uses new public setter
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Settings")) {
                app.SetShowSettingsRequested(true);  // Uses new public setter
            }
            if (ImGui::MenuItem("PCB Details")) {
                // Application will handle visibility logic based on whether a board is loaded
                app.SetShowPcbDetailsRequested(true);  // Uses new public setter
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo Window", nullptr, &m_show_im_gui_demo_window_);
            ImGui::MenuItem("ImGui Metrics/Debugger", nullptr, &m_show_im_gui_metrics_window_);
            ImGui::EndMenu();
        }
        // Add other menus like "Help" if needed
        ImGui::EndMainMenuBar();
    }

    // Render ImGui helper windows if their state is true
    if (m_show_im_gui_demo_window_) {
        ImGui::ShowDemoWindow(&m_show_im_gui_demo_window_);
    }
    if (m_show_im_gui_metrics_window_) {
        ImGui::ShowMetricsWindow(&m_show_im_gui_metrics_window_);
    }
}

#include "ui/MainMenuBar.hpp"
#include "core/Application.hpp" // For Application& app and its methods/flags
#include "imgui.h"

MainMenuBar::MainMenuBar() {
    // Constructor: Initialize any default states if necessary
    m_showImGuiDemoWindow = false;
    m_showImGuiMetricsWindow = false;
}

MainMenuBar::~MainMenuBar() {
    // Destructor: Cleanup if necessary
}

void MainMenuBar::RenderUI(Application& app) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open PCB File...", "Ctrl+O")) {
                app.SetOpenFileRequested(true); // Uses new public setter
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                app.SetQuitFileRequested(true); // Uses new public setter
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Settings")) {
                app.SetShowSettingsRequested(true); // Uses new public setter
            }
            if (ImGui::MenuItem("PCB Details")) {
                // Application will handle visibility logic based on whether a board is loaded
                app.SetShowPcbDetailsRequested(true); // Uses new public setter
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo Window", nullptr, &m_showImGuiDemoWindow);
            ImGui::MenuItem("ImGui Metrics/Debugger", nullptr, &m_showImGuiMetricsWindow);
            ImGui::EndMenu();
        }
        // Add other menus like "Help" if needed
        ImGui::EndMainMenuBar();
    }

    // Render ImGui helper windows if their state is true
    if (m_showImGuiDemoWindow) {
        ImGui::ShowDemoWindow(&m_showImGuiDemoWindow);
    }
    if (m_showImGuiMetricsWindow) {
        ImGui::ShowMetricsWindow(&m_showImGuiMetricsWindow);
    }
} 
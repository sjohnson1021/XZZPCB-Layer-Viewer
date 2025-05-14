#include "ui/windows/SettingsWindow.hpp"
#include "view/GridSettings.hpp" // For GridColor, GridStyle, etc.
#include "imgui_internal.h" // For ImGui::GetCurrentWindow(); (can be avoided if not strictly needed)

SettingsWindow::SettingsWindow(std::shared_ptr<GridSettings> gridSettings)
    : m_gridSettings(gridSettings) {
}

SettingsWindow::~SettingsWindow() {
}

void SettingsWindow::ShowGridSettings() {
    if (!m_gridSettings) {
        ImGui::Text("GridSettings not available.");
        return;
    }

    if (ImGui::CollapsingHeader("Grid Options", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Visible", &m_gridSettings->m_visible);
        ImGui::Checkbox("Dynamic Spacing", &m_gridSettings->m_isDynamic);
        ImGui::Checkbox("Show Axis Lines", &m_gridSettings->m_showAxisLines);

        // Grid Style
        const char* styles[] = {"Lines", "Dots"};
        int currentStyle = static_cast<int>(m_gridSettings->m_style);
        if (ImGui::Combo("Style", &currentStyle, styles, IM_ARRAYSIZE(styles))) {
            m_gridSettings->m_style = static_cast<GridStyle>(currentStyle);
        }

        // Grid Units (Display only for now, logic for unit conversion would be elsewhere)
        const char* units[] = {"Metric", "Imperial"};
        int currentUnit = static_cast<int>(m_gridSettings->m_unitSystem);
        // ImGui::Combo("Unit System", &currentUnit, units, IM_ARRAYSIZE(units)); // Read-only for now
        ImGui::Text("Unit System: %s", units[currentUnit]);

        ImGui::SeparatorText("Spacing");
        ImGui::DragFloat("Base Major Spacing", &m_gridSettings->m_baseMajorSpacing, 0.1f, 0.1f, 1000.0f, "%.2f");
        ImGui::SliderInt("Subdivisions", &m_gridSettings->m_subdivisions, 1, 100);
        
        ImGui::SeparatorText("Colors");
        ImGui::ColorEdit4("Major Line Color", &m_gridSettings->m_majorLineColor.r);
        ImGui::ColorEdit4("Minor Line Color", &m_gridSettings->m_minorLineColor.r);
        if (m_gridSettings->m_showAxisLines) {
            ImGui::ColorEdit4("X-Axis Color", &m_gridSettings->m_xAxisColor.r);
            ImGui::ColorEdit4("Y-Axis Color", &m_gridSettings->m_yAxisColor.r);
        }
    }
}

void SettingsWindow::RenderUI() {
    if (!m_isOpen) {
        return;
    }

    // Make window reasonably sized by default
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(m_windowName.c_str(), &m_isOpen)) {
        ImGui::Text("Application and View Settings");
        ImGui::Spacing();

        if (ImGui::BeginTabBar("SettingsTabs")) {
            if (ImGui::BeginTabItem("Grid")) {
                ShowGridSettings();
                ImGui::EndTabItem();
            }
            // if (ImGui::BeginTabItem("Theme")) {
            //     ShowThemeSettings(); // Placeholder for future theme settings
            //     ImGui::EndTabItem();
            // }
            // if (ImGui::BeginTabItem("Application")) {
            //     ShowApplicationSettings(); // Placeholder
            //     ImGui::EndTabItem();
            // }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
} 
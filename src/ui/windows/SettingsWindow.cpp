#include "ui/windows/SettingsWindow.hpp"

#include <vector>  // For storing actions to iterate

#include <SDL3/SDL_log.h>  // For SDL_Log

#include "imgui_internal.h"  // For ImGui::GetCurrentWindow(); (can be avoided if not strictly needed)

#include "core/BoardDataManager.hpp"  // Added
#include "core/ControlSettings.hpp"   // Added include
#include "core/InputActions.hpp"      // For InputAction enum and strings
#include "pcb/Board.hpp"              // Include Board.hpp
#include "utils/ColorUtils.hpp"       // Added
#include "view/GridSettings.hpp"      // For GridColor, GridStyle, etc.

// Remove static placeholders for layer visibility
// static bool layer_TopCopper = true;
// ... (all other static layer booleans removed)

SettingsWindow::SettingsWindow(std::shared_ptr<GridSettings> gridSettings,
                               std::shared_ptr<ControlSettings> controlSettings,
                               std::shared_ptr<BoardDataManager> boardDataManager,
                               float* applicationClearColor)  // Added applicationClearColor
    : m_gridSettings(gridSettings),
      m_controlSettings(controlSettings),
      m_boardDataManager(boardDataManager),
      m_appClearColor(applicationClearColor)  // Store the pointer
      ,
      m_isOpen(false)  // Default to closed
      ,
      m_windowName("Settings")
{  // Added constructor
}

SettingsWindow::~SettingsWindow() {}

void SettingsWindow::ShowGridSettings()
{
    if (!m_gridSettings) {
        ImGui::Text("GridSettings not available.");
        return;
    }

    if (ImGui::CollapsingHeader("Grid Options", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Visible", &m_gridSettings->m_visible);
        if (m_gridSettings->m_visible) {
            ImGui::Indent();

            // Display unit
            std::string_view unitStr = m_gridSettings->UnitToString();

            ImGui::Checkbox("Dynamic Spacing", &m_gridSettings->m_is_dynamic);
            if (m_gridSettings->m_is_dynamic) {
                ImGui::Indent();

                const char* pixelStepOptions[] = {"8", "16", "32", "64", "128", "256", "512", "1024", "2048", "4096", "8192", "16384", "32768", "65536"};
                const int numOptions = IM_ARRAYSIZE(pixelStepOptions);
                const float pixelStepValues[] = {8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f, 512.0f, 1024.0f, 2048.0f, 4096.0f, 8192.0f, 16384.0f, 32768.0f, 65536.0f};

                static int minPixelStepIndex = 0;
                static int maxPixelStepIndex = 7;  // 1024
                // Set defaults if not already set
                if (m_gridSettings->m_min_pixel_step != pixelStepValues[minPixelStepIndex]) {
                    for (int i = 0; i < numOptions; i++) {
                        if (m_gridSettings->m_min_pixel_step <= pixelStepValues[i]) {
                            minPixelStepIndex = i;
                            break;
                        }
                    }
                }
                if (m_gridSettings->m_max_pixel_step != pixelStepValues[maxPixelStepIndex]) {
                    for (int i = 0; i < numOptions; i++) {
                        if (m_gridSettings->m_max_pixel_step <= pixelStepValues[i]) {
                            maxPixelStepIndex = i;
                            break;
                        }
                    }
                }

                if (ImGui::Combo("Min Pixel Step", &minPixelStepIndex, pixelStepOptions, numOptions)) {
                    m_gridSettings->m_min_pixel_step = pixelStepValues[minPixelStepIndex];
                    if (m_gridSettings->m_max_pixel_step < m_gridSettings->m_min_pixel_step) {
                        m_gridSettings->m_max_pixel_step = m_gridSettings->m_min_pixel_step;
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Minimum pixel gap on screen for grid lines.\nMajor lines will adapt their world spacing.\nMinor lines will be hidden or their count reduced if they become denser than this.");
                }

                if (ImGui::Combo("Max Pixel Step", &maxPixelStepIndex, pixelStepOptions, numOptions)) {
                    m_gridSettings->m_max_pixel_step = pixelStepValues[maxPixelStepIndex];
                    if (m_gridSettings->m_min_pixel_step > m_gridSettings->m_max_pixel_step) {
                        m_gridSettings->m_min_pixel_step = m_gridSettings->m_max_pixel_step;
                    }
                }

                ImGui::Unindent();
            }

            // Adjust spacing input based on unit system
            if (m_gridSettings->m_unit_system == GridUnitSystem::kMetric) {
                // For metric, allow 1mm to 1000mm (1m) major spacing
                ImGui::DragFloat(("Major Spacing (" + std::string(unitStr) + ")").c_str(), &m_gridSettings->m_base_major_spacing, 1.0f, 1.0f, 1000.0f, "%.2f");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Major grid line spacing in millimeters.\nCommon values: 1, 2, 5, 10, 20, 50, 100mm");
                }
            } else {
                // For imperial, allow 0.1" to 36" (3 feet) major spacing
                ImGui::DragFloat(("Major Spacing (" + std::string(unitStr) + ")").c_str(), &m_gridSettings->m_base_major_spacing, 0.125f, 0.1f, 36.0f, "%.3f");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Major grid line spacing in inches.\nCommon values: 0.1, 0.25, 0.5, 1, 2, 6, 12 inches");
                }
            }

            // Adjust subdivision options based on unit system
            const char* subdivisionsLabel = m_gridSettings->m_is_dynamic ? "Maximum Subdivisions" : "Subdivisions";

            if (m_gridSettings->m_unit_system == GridUnitSystem::kMetric) {
                // For metric, prefer 1, 2, 5, 10 subdivisions
                const char* metricSubdivisionOptions[] = {"1", "2", "5", "10"};
                const int metricSubdivisionValues[] = {1, 2, 5, 10};
                int currentMetricSubIndex = 0;

                // Find closest match
                for (int i = 0; i < 4; i++) {
                    if (m_gridSettings->m_subdivisions <= metricSubdivisionValues[i]) {
                        currentMetricSubIndex = i;
                        break;
                    }
                }

                if (ImGui::Combo(subdivisionsLabel, &currentMetricSubIndex, metricSubdivisionOptions, 4)) {
                    m_gridSettings->m_subdivisions = metricSubdivisionValues[currentMetricSubIndex];
                }
            } else {
                // For imperial, prefer powers of 2: 1, 2, 4, 8, 16
                const char* imperialSubdivisionOptions[] = {"1", "2", "4", "8", "16"};
                const int imperialSubdivisionValues[] = {1, 2, 4, 8, 16};
                int currentImperialSubIndex = 0;

                // Find closest match
                for (int i = 0; i < 5; i++) {
                    if (m_gridSettings->m_subdivisions <= imperialSubdivisionValues[i]) {
                        currentImperialSubIndex = i;
                        break;
                    }
                }

                if (ImGui::Combo(subdivisionsLabel, &currentImperialSubIndex, imperialSubdivisionOptions, 5)) {
                    m_gridSettings->m_subdivisions = imperialSubdivisionValues[currentImperialSubIndex];
                }
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Desired number of subdivisions between major grid lines.\nWhen Dynamic Spacing is on, the actual number of minor lines drawn may be less if they become too dense "
                                  "on screen (see Min Pixel Step).");
            }

            // Style selection
            const char* styles[] = {"Lines", "Dots"};
            int currentStyle = static_cast<int>(m_gridSettings->m_style);
            if (ImGui::Combo("Style", &currentStyle, styles, IM_ARRAYSIZE(styles))) {
                m_gridSettings->m_style = static_cast<GridStyle>(currentStyle);
            }

            // Show measurement readout option
            ImGui::Checkbox("Show Measurement Readout", &m_gridSettings->m_show_measurement_readout);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Display current grid spacing measurements on screen");
            }

            ImGui::SeparatorText("Colors");
            ImGui::ColorEdit4("Major Lines", &m_gridSettings->m_major_line_color.r);
            ImGui::ColorEdit4("Minor Lines", &m_gridSettings->m_minor_line_color.r);
            ImGui::ColorEdit4("Grid Background", &m_gridSettings->m_background_color.r);

            ImGui::Checkbox("Show Axis Lines", &m_gridSettings->m_show_axis_lines);
            if (m_gridSettings->m_show_axis_lines) {
                ImGui::Indent();
                ImGui::ColorEdit4("X-Axis Color", &m_gridSettings->m_x_axis_color.r);
                ImGui::ColorEdit4("Y-Axis Color", &m_gridSettings->m_y_axis_color.r);
                ImGui::Unindent();
            }

            ImGui::SeparatorText("Performance Limits");
            ImGui::TextWrapped("Maximum renderable grid lines: %d", m_gridSettings->kMaxRenderableLines);
            ImGui::TextWrapped("Maximum renderable grid dots: %d", m_gridSettings->kMaxRenderableDots);

            ImGui::Unindent();
        }
    }
}

namespace
{
// Helper to capture the next key press for binding
KeyCombination CaptureKeybind()
{
    KeyCombination newKeybind = {};
    ImGuiIO& io = ImGui::GetIO();

    for (ImGuiKey key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key = (ImGuiKey) (key + 1)) {
        if (ImGui::IsKeyPressed(key, false)) {  // `false` for `ImGuiKeyOwner_Any`, check if key was pressed this frame
            newKeybind.key = key;
            break;
        }
    }
    // Also check for modifier keys that might be held *with* a non-modifier key press handled above
    // or if only a modifier is pressed (though less common for a primary keybind key).
    if (newKeybind.key != ImGuiKey_None) {  // Only set modifiers if a main key was pressed
        newKeybind.ctrl = io.KeyCtrl;
        newKeybind.shift = io.KeyShift;
        newKeybind.alt = io.KeyAlt;
    } else {
        // Fallback for modifier-only or special keys if needed, e.g. if a modifier itself is the bind.
        // For now, require a non-modifier key to be part of the bind.
    }
    return newKeybind;
}
}  // namespace

void SettingsWindow::ShowControlSettings()
{
    if (!m_controlSettings) {
        ImGui::Text("ControlSettings not available.");
        return;
    }

    if (ImGui::CollapsingHeader("Navigation Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Free Camera Rotation (Hold Key)", &m_controlSettings->m_freeRotation);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("If enabled, holding Q/E continuously rotates the camera.\nIf disabled, Q/E will snap rotation by 90 degrees per press.");
            ImGui::EndTooltip();
        }

        ImGui::Checkbox("Rotate Around Cursor", &m_controlSettings->m_rotateAroundCursor);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If enabled, keyboard rotation will pivot around the mouse cursor position (if over the viewport). Otherwise, it pivots around the viewport center.");
        }
        ImGui::InputFloat("Snap Rotation Angle", &m_controlSettings->m_snapRotationAngle, 1.0f, 5.0f, "%.1f deg");
        if (m_controlSettings->m_snapRotationAngle < 1.0f)
            m_controlSettings->m_snapRotationAngle = 1.0f;
        if (m_controlSettings->m_snapRotationAngle > 180.0f)
            m_controlSettings->m_snapRotationAngle = 180.0f;
    }

    ImGui::SeparatorText("Keybinds");
    if (ImGui::CollapsingHeader("Application Keybinds", ImGuiTreeNodeFlags_None)) {
        ImGui::TextWrapped("Click 'Set' then press the desired key combination for an action. Click again to cancel.");
        ImGui::Spacing();

        if (ImGui::Button("Reset All Keybinds to Default")) {
            m_controlSettings->ResetKeybindsToDefault();
            // Potentially mark config as dirty to be saved
        }
        ImGui::Spacing();

        if (ImGui::BeginTable("keybindsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Keybind", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            static InputAction actionToRebind = InputAction::Count;  // Special value indicating no rebind active
            static std::string rebindButtonText = "Set";

            for (int i = 0; i < static_cast<int>(InputAction::Count); ++i) {
                InputAction currentAction = static_cast<InputAction>(i);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(InputActionToString(currentAction));

                ImGui::TableSetColumnIndex(1);
                KeyCombination currentKb = m_controlSettings->GetKeybind(currentAction);

                std::string buttonLabel = "Set";
                if (actionToRebind == currentAction) {
                    buttonLabel = "Capturing... Click to Cancel";
                } else {
                    buttonLabel = currentKb.ToString();
                }

                ImGui::PushID(i);                                               // Unique ID for each button
                if (ImGui::Button(buttonLabel.c_str(), ImVec2(-FLT_MIN, 0))) {  // -FLT_MIN makes button stretch
                    if (actionToRebind == currentAction) {                      // Was capturing, now cancel
                        actionToRebind = InputAction::Count;
                    } else {  // Start capturing for this action
                        actionToRebind = currentAction;
                        ImGui::SetKeyboardFocusHere();  // Try to focus to capture keys, may not be fully effective
                    }
                }
                ImGui::PopID();

                if (actionToRebind == currentAction) {
                    ImGui::SetItemDefaultFocus();  // Try to keep focus for key capture
                    KeyCombination newKb = CaptureKeybind();
                    if (newKb.IsBound()) {
                        // Check for conflicts (optional, basic check here)
                        bool conflict = false;
                        for (int j = 0; j < static_cast<int>(InputAction::Count); ++j) {
                            if (i == j)
                                continue;
                            if (m_controlSettings->GetKeybind(static_cast<InputAction>(j)) == newKb) {
                                // TODO: Show a more user-friendly conflict warning
                                SDL_Log("Keybind %s already used for %s", newKb.ToString().c_str(), InputActionToString(static_cast<InputAction>(j)));
                                conflict = true;
                                break;
                            }
                        }
                        if (!conflict) {  // Or if user confirms overwrite
                            m_controlSettings->SetKeybind(currentAction, newKb);
                        }  // else, newKb is ignored due to conflict / or user cancels
                        actionToRebind = InputAction::Count;  // Stop capturing
                    } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered()) {
                        // If clicked outside the button while capturing, cancel capture.
                        // This logic might need refinement depending on ImGui behavior.
                        actionToRebind = InputAction::Count;
                    }
                }
            }
            ImGui::EndTable();
        }
    }
}

void SettingsWindow::ShowLayerControls(const std::shared_ptr<Board>& currentBoard)
{  // Updated signature
    // ImGui::SeparatorText("PCB Layer Visibility"); // Combined into Layer Styling

    if (!currentBoard) {
        ImGui::TextDisabled("No board loaded. Layer controls unavailable.");
        return;
    }

    if (currentBoard->GetLayerCount() == 0) {
        ImGui::TextDisabled("Board has no layers defined.");
        return;
    }

    // Assuming Board has an interface like:
    // int GetLayerCount() const;
    // std::string GetLayerName(int layerIndex) const;
    // bool IsLayerVisible(int layerIndex) const;
    // void SetLayerVisible(int layerIndex, bool visible);
    // And that modifying visibility through SetLayerVisible might trigger a redraw elsewhere.

    for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
        std::string layerName = currentBoard->GetLayerName(i);
        if (layerName.empty()) {
            layerName = "Unnamed Layer " + std::to_string(i);
        }
        bool layerVisible = currentBoard->IsLayerVisible(i);

        if (ImGui::Checkbox(layerName.c_str(), &layerVisible)) {
            currentBoard->SetLayerVisible(i, layerVisible);
            // Optionally, mark the board as dirty or trigger an event if needed
        }
    }
}

void SettingsWindow::ShowAppearanceSettings(const std::shared_ptr<Board>& currentBoard)
{
    if (!m_appClearColor) {
        ImGui::Text("Application clear color not available.");
    } else {
        ImGui::SeparatorText("Application Appearance");
        ImGui::ColorEdit3("Background Color", m_appClearColor, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB);
        ImGui::Spacing();
    }

    // TODO: Move these to their own sections

    ImGui::SeparatorText("Board Colors");
    // Net Highlighting
    BLRgba32 highlightColor = m_boardDataManager->GetColor(BoardDataManager::ColorType::kNetHighlight);
    float colorArr_NetHighlightColor[4] = {highlightColor.r() / 255.0f, highlightColor.g() / 255.0f, highlightColor.b() / 255.0f, highlightColor.a() / 255.0f};
    if (ImGui::ColorEdit4("Net Highlight Color", colorArr_NetHighlightColor, ImGuiColorEditFlags_Float)) {
        m_boardDataManager->SetColor(BoardDataManager::ColorType::kNetHighlight,
                                     BLRgba32(static_cast<uint32_t>(colorArr_NetHighlightColor[0] * 255),
                                              static_cast<uint32_t>(colorArr_NetHighlightColor[1] * 255),
                                              static_cast<uint32_t>(colorArr_NetHighlightColor[2] * 255),
                                              static_cast<uint32_t>(colorArr_NetHighlightColor[3] * 255)));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Color used to highlight elements that belong to the selected net.");
    }
    ImGui::Spacing();

    // Silkscreen Colors
    BLRgba32 silkscreenColor = m_boardDataManager->GetColor(BoardDataManager::ColorType::kSilkscreen);
    float colorArr_SilkscreenColor[4] = {silkscreenColor.r() / 255.0f, silkscreenColor.g() / 255.0f, silkscreenColor.b() / 255.0f, silkscreenColor.a() / 255.0f};
    if (ImGui::ColorEdit4("Silkscreen Color", colorArr_SilkscreenColor, ImGuiColorEditFlags_Float)) {
        m_boardDataManager->SetColor(BoardDataManager::ColorType::kSilkscreen,
                                     BLRgba32(static_cast<uint32_t>(colorArr_SilkscreenColor[0] * 255),
                                              static_cast<uint32_t>(colorArr_SilkscreenColor[1] * 255),
                                              static_cast<uint32_t>(colorArr_SilkscreenColor[2] * 255),
                                              static_cast<uint32_t>(colorArr_SilkscreenColor[3] * 255)));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Color used to render silkscreen elements.");
    }
    ImGui::Spacing();

    // Component Colors
    BLRgba32 componentColor = m_boardDataManager->GetColor(BoardDataManager::ColorType::kComponent);
    float colorArr_ComponentColor[4] = {componentColor.r() / 255.0f, componentColor.g() / 255.0f, componentColor.b() / 255.0f, componentColor.a() / 255.0f};
    if (ImGui::ColorEdit4("Component Color", colorArr_ComponentColor, ImGuiColorEditFlags_Float)) {
        m_boardDataManager->SetColor(BoardDataManager::ColorType::kComponent,
                                     BLRgba32(static_cast<uint32_t>(colorArr_ComponentColor[0] * 255),
                                              static_cast<uint32_t>(colorArr_ComponentColor[1] * 255),
                                              static_cast<uint32_t>(colorArr_ComponentColor[2] * 255),
                                              static_cast<uint32_t>(colorArr_ComponentColor[3] * 255)));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Color used to render components.");
    }

    ImGui::Spacing();

    // Pin Colors
    BLRgba32 pinColor = m_boardDataManager->GetColor(BoardDataManager::ColorType::kPin);
    float colorArr_PinColor[4] = {pinColor.r() / 255.0f, pinColor.g() / 255.0f, pinColor.b() / 255.0f, pinColor.a() / 255.0f};
    if (ImGui::ColorEdit4("Pin Color", colorArr_PinColor, ImGuiColorEditFlags_Float)) {
        m_boardDataManager->SetColor(BoardDataManager::ColorType::kPin,
                                     BLRgba32(static_cast<uint32_t>(colorArr_PinColor[0] * 255),
                                              static_cast<uint32_t>(colorArr_PinColor[1] * 255),
                                              static_cast<uint32_t>(colorArr_PinColor[2] * 255),
                                              static_cast<uint32_t>(colorArr_PinColor[3] * 255)));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Color used to render pins.");
    }

    ImGui::Spacing();

    // Board Edges Colors

    BLRgba32 boardEdgesColor = m_boardDataManager->GetColor(BoardDataManager::ColorType::kBoardEdges);
    float colorArr_BoardEdgesColor[4] = {boardEdgesColor.r() / 255.0f, boardEdgesColor.g() / 255.0f, boardEdgesColor.b() / 255.0f, boardEdgesColor.a() / 255.0f};
    if (ImGui::ColorEdit4("Board Edges Color", colorArr_BoardEdgesColor, ImGuiColorEditFlags_Float)) {
        m_boardDataManager->SetColor(BoardDataManager::ColorType::kBoardEdges,
                                     BLRgba32(static_cast<uint32_t>(colorArr_BoardEdgesColor[0] * 255),
                                              static_cast<uint32_t>(colorArr_BoardEdgesColor[1] * 255),
                                              static_cast<uint32_t>(colorArr_BoardEdgesColor[2] * 255),
                                              static_cast<uint32_t>(colorArr_BoardEdgesColor[3] * 255)));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Color used to render board edges.");
    }

    ImGui::Spacing();

    // Base Layer Color
    BLRgba32 baseColor = m_boardDataManager->GetColor(BoardDataManager::ColorType::kBaseLayer);
    float colorArr_BaseColor[4] = {baseColor.r() / 255.0f, baseColor.g() / 255.0f, baseColor.b() / 255.0f, baseColor.a() / 255.0f};
    if (ImGui::ColorEdit4("Base Layer Color", colorArr_BaseColor, ImGuiColorEditFlags_Float)) {
        m_boardDataManager->SetColor(BoardDataManager::ColorType::kBaseLayer,
                                     BLRgba32(static_cast<uint32_t>(colorArr_BaseColor[0] * 255),
                                              static_cast<uint32_t>(colorArr_BaseColor[1] * 255),
                                              static_cast<uint32_t>(colorArr_BaseColor[2] * 255),
                                              static_cast<uint32_t>(colorArr_BaseColor[3] * 255)));
        m_boardDataManager->RegenerateLayerColors(currentBoard);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The starting color for the first layer. Subsequent layers will have their hue shifted from this color.");
    }

    // Hue Step per Layer
    float hueStep = m_boardDataManager->GetLayerHueStep();
    if (ImGui::DragFloat("Hue Shift per Layer", &hueStep, 1.0f, 0.0f, 180.0f, "%.1f degrees")) {
        m_boardDataManager->SetLayerHueStep(hueStep);
        m_boardDataManager->RegenerateLayerColors(currentBoard);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("How much the hue is shifted for each subsequent layer, in degrees.");
    }
    ImGui::Spacing();

    ShowLayerControls(currentBoard);  // Pass currentBoard
}

void SettingsWindow::RenderUI(const std::shared_ptr<Board>& currentBoard)
{  // Added currentBoard parameter back
    if (!m_isOpen) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(450, 550), ImGuiCond_FirstUseEver);

    bool window_open = ImGui::Begin(m_windowName.c_str(), &m_isOpen);

    if (!window_open) {
        ImGui::End();
        return;
    }

    // Get the current board from BoardDataManager to pass to relevant sections -- REMOVED, board is now passed in
    // std::shared_ptr<Board> currentBoard = m_boardDataManager.getBoard();

    if (ImGui::BeginTabBar("SettingsTabs")) {
        if (ImGui::BeginTabItem("Display")) {
            ShowAppearanceSettings(currentBoard);  // Pass currentBoard
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Grid")) {
            ShowGridSettings();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controls")) {
            ShowControlSettings();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();  // This End matches the successful Begin
}

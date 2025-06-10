#include "ui/windows/SettingsWindow.hpp"

#include <algorithm>  // For std::max, std::min
#include <iostream>

#include "imgui_internal.h"  // For ImGui::GetCurrentWindow(); (can be avoided if not strictly needed)

#include "core/BoardDataManager.hpp"  // Added
#include "core/Config.hpp"            // For font settings persistence
#include "core/ControlSettings.hpp"   // Added include
#include "core/InputActions.hpp"      // For InputAction enum and strings
#include "pcb/Board.hpp"              // Include Board.hpp
#include "utils/ColorUtils.hpp"       // Added
#include "view/Grid.hpp"              // For Grid font invalidation
#include "view/GridSettings.hpp"      // For GridColor, GridStyle, etc.

SettingsWindow::SettingsWindow(std::shared_ptr<GridSettings> grid_settings,
                               std::shared_ptr<ControlSettings> control_settings,
                               std::shared_ptr<BoardDataManager> board_data_manager,
                               float* application_clear_color,  // Added applicationClearColor
                               std::shared_ptr<Grid> grid)  // Added Grid for font invalidation
    : m_grid_settings_(grid_settings),
      m_control_settings_(control_settings),
      m_board_data_manager_(board_data_manager),
      m_app_clear_color_(application_clear_color),  // Store the pointer
      m_grid_(grid),  // Store the Grid instance
      m_is_open_(false),  // Default to closed
      m_window_name("Settings"),
      m_font_scale_multiplier_(1.0f),
      m_font_settings_changed_(false)
{  // Added constructor
}

SettingsWindow::~SettingsWindow() {}

void SettingsWindow::ShowGridSettings()
{
    if (!m_grid_settings_) {
        ImGui::Text("GridSettings not available.");
        return;
    }

    if (ImGui::CollapsingHeader("Grid Options", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Visible", &m_grid_settings_->m_visible);
        if (m_grid_settings_->m_visible) {
            ImGui::Indent();

            // Unit System Selection
            const char* unitSystems[] = {"Metric (mm)","Imperial (inches)"};
            int currentUnitSystem = static_cast<int>(m_grid_settings_->m_unit_system);
            if (ImGui::Combo("Unit System", &currentUnitSystem, unitSystems, IM_ARRAYSIZE(unitSystems))) {
                GridUnitSystem newUnitSystem = static_cast<GridUnitSystem>(currentUnitSystem);
                if (newUnitSystem != m_grid_settings_->m_unit_system) {
                    GridUnitSystem oldUnitSystem = m_grid_settings_->m_unit_system;
                    m_grid_settings_->m_unit_system = newUnitSystem;

                    // Convert spacing values to clean, common values in the new unit system
                    if (newUnitSystem == GridUnitSystem::kImperial && oldUnitSystem == GridUnitSystem::kMetric) {
                        // Converting from Metric to Imperial
                        // First convert world units to mm, then mm to inches, then get clean imperial value
                        float mmValue = GridSettings::WorldUnitsToMm(m_grid_settings_->m_base_major_spacing);
                        float inchesValue = GridSettings::MmToInches(mmValue);
                        float cleanInches = GridSettings::GetCleanImperialSpacing(inchesValue);
                        m_grid_settings_->m_base_major_spacing = GridSettings::InchesToWorldUnits(cleanInches);
                        m_grid_settings_->m_subdivisions = 10;  // Common for imperial
                    } else if (newUnitSystem == GridUnitSystem::kMetric && oldUnitSystem == GridUnitSystem::kImperial) {
                        // Converting from Imperial to Metric
                        // First convert world units to inches, then inches to mm, then get clean metric value
                        float inchesValue = GridSettings::WorldUnitsToInches(m_grid_settings_->m_base_major_spacing);
                        float mmValue = GridSettings::InchesToMm(inchesValue);
                        float cleanMm = GridSettings::GetCleanMetricSpacing(mmValue);
                        m_grid_settings_->m_base_major_spacing = GridSettings::MmToWorldUnits(cleanMm);
                        m_grid_settings_->m_subdivisions = 10;  // Standard for metric (gives 0.5mm minor spacing for 5mm major)
                    }

                    std::cout << "Grid unit system changed to " << (newUnitSystem == GridUnitSystem::kImperial ? "Imperial" : "Metric")
                              << ", spacing adjusted to " << m_grid_settings_->m_base_major_spacing << std::endl;
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Choose between Imperial (inches) and Metric (millimeters) units.\nXZZ files natively use thousandths of an inch (mils).\nSpacing values will be converted to clean, common values.");
            }

            // Display unit
            std::string_view unitStr = m_grid_settings_->UnitToString();

            ImGui::Checkbox("Dynamic Spacing", &m_grid_settings_->m_is_dynamic);
            if (m_grid_settings_->m_is_dynamic) {
                ImGui::Indent();

                const char* pixelStepOptions[] = {"8", "16", "32", "64", "128", "256", "512", "1024", "2048", "4096", "8192", "16384", "32768", "65536"};
                const int numOptions = IM_ARRAYSIZE(pixelStepOptions);
                const float pixelStepValues[] = {8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f, 512.0f, 1024.0f, 2048.0f, 4096.0f, 8192.0f, 16384.0f, 32768.0f, 65536.0f};

                static int minPixelStepIndex = 0;
                static int maxPixelStepIndex = 7;  // 1024
                // Set defaults if not already set
                if (m_grid_settings_->m_min_pixel_step != pixelStepValues[minPixelStepIndex]) {
                    for (int i = 0; i < numOptions; i++) {
                        if (m_grid_settings_->m_min_pixel_step <= pixelStepValues[i]) {
                            minPixelStepIndex = i;
                            break;
                        }
                    }
                }
                if (m_grid_settings_->m_max_pixel_step != pixelStepValues[maxPixelStepIndex]) {
                    for (int i = 0; i < numOptions; i++) {
                        if (m_grid_settings_->m_max_pixel_step <= pixelStepValues[i]) {
                            maxPixelStepIndex = i;
                            break;
                        }
                    }
                }

                if (ImGui::Combo("Min Pixel Step", &minPixelStepIndex, pixelStepOptions, numOptions)) {
                    m_grid_settings_->m_min_pixel_step = pixelStepValues[minPixelStepIndex];
                    if (m_grid_settings_->m_max_pixel_step < m_grid_settings_->m_min_pixel_step) {
                        m_grid_settings_->m_max_pixel_step = m_grid_settings_->m_min_pixel_step;
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Minimum pixel gap on screen for grid lines.\nMajor lines will adapt their world spacing.\nMinor lines will be hidden or their count reduced if they become denser than this.");
                }

                if (ImGui::Combo("Max Pixel Step", &maxPixelStepIndex, pixelStepOptions, numOptions)) {
                    m_grid_settings_->m_max_pixel_step = pixelStepValues[maxPixelStepIndex];
                    if (m_grid_settings_->m_min_pixel_step > m_grid_settings_->m_max_pixel_step) {
                        m_grid_settings_->m_min_pixel_step = m_grid_settings_->m_max_pixel_step;
                    }
                }

                ImGui::Unindent();
            }

            // Spacing input controls - convert between world coordinates and display units
            if (m_grid_settings_->m_unit_system == GridUnitSystem::kMetric) {
                // Convert world units to mm for display
                float displayValue = GridSettings::WorldUnitsToMm(m_grid_settings_->m_base_major_spacing);

                // For metric, allow 0.1mm to 1000mm (1m) major spacing
                if (ImGui::DragFloat(("Major Spacing (" + std::string(unitStr) + ")").c_str(), &displayValue, 0.5f, 0.1f, 1000.0f, "%.1f")) {
                    // Convert back to world units when changed
                    m_grid_settings_->m_base_major_spacing = GridSettings::MmToWorldUnits(displayValue);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Major grid line spacing in millimeters.\nCommon values: 1, 2.5, 5, 10, 25, 50, 100, 250mm");
                }
            } else {
                // Convert world units to inches for display
                float displayValue = GridSettings::WorldUnitsToInches(m_grid_settings_->m_base_major_spacing);

                // For imperial, allow 0.01" to 36" (3 feet) major spacing
                if (ImGui::DragFloat(("Major Spacing (" + std::string(unitStr) + ")").c_str(), &displayValue, 0.01f, 0.01f, 36.0f, "%.3f")) {
                    // Convert back to world units when changed
                    m_grid_settings_->m_base_major_spacing = GridSettings::InchesToWorldUnits(displayValue);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Major grid line spacing in inches.\nCommon values: 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2, 4, 6, 12 inches");
                }
            }

            // Adjust subdivision options based on unit system
            const char* subdivisionsLabel = m_grid_settings_->m_is_dynamic ? "Maximum Subdivisions" : "Subdivisions";

            if (m_grid_settings_->m_unit_system == GridUnitSystem::kMetric) {
                // For metric, prefer 1, 2, 5, 10 subdivisions
                const char* metricSubdivisionOptions[] = {"1", "2", "5", "10"};
                const int metricSubdivisionValues[] = {1, 2, 5, 10};
                int currentMetricSubIndex = 0;

                // Find closest match
                for (int i = 0; i < 4; i++) {
                    if (m_grid_settings_->m_subdivisions <= metricSubdivisionValues[i]) {
                        currentMetricSubIndex = i;
                        break;
                    }
                }

                if (ImGui::Combo(subdivisionsLabel, &currentMetricSubIndex, metricSubdivisionOptions, 4)) {
                    m_grid_settings_->m_subdivisions = metricSubdivisionValues[currentMetricSubIndex];
                }
            } else {
                // For imperial, prefer powers of 2 and common values: 1, 2, 4, 8, 10, 16
                const char* imperialSubdivisionOptions[] = {"1", "2", "4", "8", "10", "16"};
                const int imperialSubdivisionValues[] = {1, 2, 4, 8, 10, 16};
                const int numImperialOptions = 6;
                int currentImperialSubIndex = 0;

                // Find closest match
                for (int i = 0; i < numImperialOptions; i++) {
                    if (m_grid_settings_->m_subdivisions <= imperialSubdivisionValues[i]) {
                        currentImperialSubIndex = i;
                        break;
                    }
                }

                if (ImGui::Combo(subdivisionsLabel, &currentImperialSubIndex, imperialSubdivisionOptions, numImperialOptions)) {
                    m_grid_settings_->m_subdivisions = imperialSubdivisionValues[currentImperialSubIndex];
                }
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Desired number of subdivisions between major grid lines.\nWhen Dynamic Spacing is on, the actual number of minor lines drawn may be less if they become too dense "
                                  "on screen (see Min Pixel Step).");
            }

            // Style selection
            const char* styles[] = {"Lines", "Dots"};
            int currentStyle = static_cast<int>(m_grid_settings_->m_style);
            if (ImGui::Combo("Style", &currentStyle, styles, IM_ARRAYSIZE(styles))) {
                m_grid_settings_->m_style = static_cast<GridStyle>(currentStyle);
            }

            // Show measurement readout option
            ImGui::Checkbox("Show Measurement Readout", &m_grid_settings_->m_show_measurement_readout);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Display current grid spacing measurements on screen");
            }

            ImGui::SeparatorText("Colors");
            BLRgba32& majorColor = m_grid_settings_->m_major_line_color;
            float colorArr_Major[4] = {majorColor.r() / 255.0f, majorColor.g() / 255.0f, majorColor.b() / 255.0f, majorColor.a() / 255.0f};
            if (ImGui::ColorEdit4("Major Lines", colorArr_Major)) {
                majorColor.setR(static_cast<uint32_t>(colorArr_Major[0] * 255));
                majorColor.setG(static_cast<uint32_t>(colorArr_Major[1] * 255));
                majorColor.setB(static_cast<uint32_t>(colorArr_Major[2] * 255));
                majorColor.setA(static_cast<uint32_t>(colorArr_Major[3] * 255));
                TriggerGridRedraw();
            }

            BLRgba32& minorColor = m_grid_settings_->m_minor_line_color;
            float colorArr_Minor[4] = {minorColor.r() / 255.0f, minorColor.g() / 255.0f, minorColor.b() / 255.0f, minorColor.a() / 255.0f};
            if (ImGui::ColorEdit4("Minor Lines", colorArr_Minor)) {
                minorColor.setR(static_cast<uint32_t>(colorArr_Minor[0] * 255));
                minorColor.setG(static_cast<uint32_t>(colorArr_Minor[1] * 255));
                minorColor.setB(static_cast<uint32_t>(colorArr_Minor[2] * 255));
                minorColor.setA(static_cast<uint32_t>(colorArr_Minor[3] * 255));
                TriggerGridRedraw();
            }

            BLRgba32& bgColor = m_grid_settings_->m_background_color;
            float colorArr_BG[4] = {bgColor.r() / 255.0f, bgColor.g() / 255.0f, bgColor.b() / 255.0f, bgColor.a() / 255.0f};
            if (ImGui::ColorEdit4("Grid Background", colorArr_BG)) {
                bgColor.setR(static_cast<uint32_t>(colorArr_BG[0] * 255));
                bgColor.setG(static_cast<uint32_t>(colorArr_BG[1] * 255));
                bgColor.setB(static_cast<uint32_t>(colorArr_BG[2] * 255));
                bgColor.setA(static_cast<uint32_t>(colorArr_BG[3] * 255));
                TriggerGridRedraw();
            }

            ImGui::Checkbox("Show Axis Lines", &m_grid_settings_->m_show_axis_lines);
            if (m_grid_settings_->m_show_axis_lines) {
                ImGui::Indent();
                BLRgba32& xAxisColor = m_grid_settings_->m_x_axis_color;
                float colorArr_X[4] = {xAxisColor.r() / 255.0f, xAxisColor.g() / 255.0f, xAxisColor.b() / 255.0f, xAxisColor.a() / 255.0f};
                if (ImGui::ColorEdit4("X-Axis Color", colorArr_X)) {
                    xAxisColor.setR(static_cast<uint32_t>(colorArr_X[0] * 255));
                    xAxisColor.setG(static_cast<uint32_t>(colorArr_X[1] * 255));
                    xAxisColor.setB(static_cast<uint32_t>(colorArr_X[2] * 255));
                    xAxisColor.setA(static_cast<uint32_t>(colorArr_X[3] * 255));
                    TriggerGridRedraw();
                }
                BLRgba32& yAxisColor = m_grid_settings_->m_y_axis_color;
                float colorArr_Y[4] = {yAxisColor.r() / 255.0f, yAxisColor.g() / 255.0f, yAxisColor.b() / 255.0f, yAxisColor.a() / 255.0f};
                if (ImGui::ColorEdit4("Y-Axis Color", colorArr_Y)) {
                    yAxisColor.setR(static_cast<uint32_t>(colorArr_Y[0] * 255));
                    yAxisColor.setG(static_cast<uint32_t>(colorArr_Y[1] * 255));
                    yAxisColor.setB(static_cast<uint32_t>(colorArr_Y[2] * 255));
                    yAxisColor.setA(static_cast<uint32_t>(colorArr_Y[3] * 255));
                    TriggerGridRedraw();
                }
                ImGui::Unindent();
            }

            ImGui::SeparatorText("Performance Limits");
            ImGui::TextWrapped("Maximum renderable grid lines: %d", m_grid_settings_->kMaxRenderableLines);
            ImGui::TextWrapped("Maximum renderable grid dots: %d", m_grid_settings_->kMaxRenderableDots);

            ImGui::Unindent();
        }
    }
}

namespace
{
// Helper to capture the next key press for binding
KeyCombination CaptureKeybind()
{
    KeyCombination new_keybind = {};
    ImGuiIO& io = ImGui::GetIO();

    // Check for non-modifier keys first
    for (ImGuiKey key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key = (ImGuiKey) (key + 1)) {
        // Skip modifier keys - we don't want them as the primary key
        if (key == ImGuiKey_ModCtrl || key == ImGuiKey_ModShift || key == ImGuiKey_ModAlt || key == ImGuiKey_ModSuper ||
			key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl ||
            key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
            key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt ||
            key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper) {
            continue;
        }

        if (ImGui::IsKeyPressed(key, false)) {  // `false` for `ImGuiKeyOwner_Any`, check if key was pressed this frame
            new_keybind.key = key;
            // Capture modifier states when a non-modifier key is pressed
            new_keybind.ctrl = io.KeyCtrl;
            new_keybind.shift = io.KeyShift;
            new_keybind.alt = io.KeyAlt;
            break;
        }
    }

    return new_keybind;
}
}  // namespace

void SettingsWindow::ShowControlSettings()
{
    if (!m_control_settings_) {
        ImGui::Text("ControlSettings not available.");
        return;
    }

    if (ImGui::CollapsingHeader("Navigation Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Free Camera Rotation (Hold Key)", &m_control_settings_->m_free_rotation);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("If enabled, holding Q/E continuously rotates the camera.\nIf disabled, Q/E will snap rotation by 90 degrees per press.");
            ImGui::EndTooltip();
        }

        ImGui::Checkbox("Rotate Around Cursor", &m_control_settings_->m_rotate_around_cursor);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If enabled, keyboard rotation will pivot around the mouse cursor position (if over the viewport). Otherwise, it pivots around the viewport center.");
        }
        ImGui::InputFloat("Snap Rotation Angle", &m_control_settings_->m_snap_rotation_angle, 1.0f, 5.0f, "%.1f deg");
        if (m_control_settings_->m_snap_rotation_angle < 1.0f)
            m_control_settings_->m_snap_rotation_angle = 1.0f;
        if (m_control_settings_->m_snap_rotation_angle > 180.0f)
            m_control_settings_->m_snap_rotation_angle = 180.0f;

        ImGui::Spacing();
        ImGui::SeparatorText("Speed Controls");

        // Zoom sensitivity control
        ImGui::SliderFloat("Zoom Sensitivity", &m_control_settings_->m_zoom_sensitivity, 1.05f, 2.0f, "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls how fast the mouse wheel zooms. Lower values = finer control, higher values = faster zooming.");
        }

        // Pan speed multiplier control
        ImGui::SliderFloat("Pan Speed", &m_control_settings_->m_pan_speed_multiplier, 0.1f, 5.0f, "%.1fx");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls keyboard pan speed. Lower values = slower panning, higher values = faster panning.");
        }
    }

    ImGui::SeparatorText("Keybinds");
    if (ImGui::CollapsingHeader("Application Keybinds", ImGuiTreeNodeFlags_None)) {
        ImGui::TextWrapped("Click 'Set' then press the desired key combination for an action. Click again to cancel.");
        ImGui::Spacing();

        if (ImGui::Button("Reset All Keybinds to Default")) {
            m_control_settings_->ResetKeybindsToDefault();
            // Potentially mark config as dirty to be saved
        }
        ImGui::Spacing();

        if (ImGui::BeginTable("keybindsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Keybind", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            static InputAction actionToRebind = InputAction::kCount;  // Special value indicating no rebind active
            static std::string rebindButtonText = "Set";

            for (int i = 0; i < static_cast<int>(InputAction::kCount); ++i) {
                InputAction currentAction = static_cast<InputAction>(i);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(InputActionToString(currentAction));

                ImGui::TableSetColumnIndex(1);
                KeyCombination currentKb = m_control_settings_->GetKeybind(currentAction);

                std::string buttonLabel = "Set";
                if (actionToRebind == currentAction) {
                    buttonLabel = "Capturing... Click to Cancel";
                } else {
                    buttonLabel = currentKb.ToString();
                }

                ImGui::PushID(i);                                               // Unique ID for each button
                if (ImGui::Button(buttonLabel.c_str(), ImVec2(-FLT_MIN, 0))) {  // -FLT_MIN makes button stretch
                    if (actionToRebind == currentAction) {                      // Was capturing, now cancel
                        actionToRebind = InputAction::kCount;
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
                        for (int j = 0; j < static_cast<int>(InputAction::kCount); ++j) {
                            if (i == j)
                                continue;
                            if (m_control_settings_->GetKeybind(static_cast<InputAction>(j)) == newKb) {
                                // TODO: Show a more user-friendly conflict warning
                                std::cout << "Keybind " << newKb.ToString() << " already used for " << InputActionToString(static_cast<InputAction>(j)) << std::endl;
                                conflict = true;
                                break;
                            }
                        }
                        if (!conflict) {  // Or if user confirms overwrite
                            m_control_settings_->SetKeybind(currentAction, newKb);
                        }  // else, newKb is ignored due to conflict / or user cancels
                        actionToRebind = InputAction::kCount;  // Stop capturing
                    } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered()) {
                        // If clicked outside the button while capturing, cancel capture.
                        // This logic might need refinement depending on ImGui behavior.
                        actionToRebind = InputAction::kCount;
                    }
                }
            }
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Interaction Priority");
    if (ImGui::CollapsingHeader("Element Selection Priority", ImGuiTreeNodeFlags_None)) {
        ImGui::TextWrapped("Drag and drop to reorder element types by selection priority. Elements higher in the list will be selected first when overlapping.");
        ImGui::Spacing();
		ImGui::Indent();
        // Get current priority order from ControlSettings
        auto current_priority = m_control_settings_->GetElementPriorityOrder();

        // Create a mutable copy for UI manipulation
        static std::array<ElementInteractionType, static_cast<size_t>(ElementInteractionType::kCount)> ui_priority_order;
        static bool initialized = false;
        if (!initialized) {
            ui_priority_order = current_priority;
            initialized = true;
        }

        ImGui::PushItemFlag(ImGuiItemFlags_AllowDuplicateId, true);

        // Calculate the available width for the priority list
        float available_width = ImGui::GetContentRegionAvail().x;
        float item_width = std::max(200.0f, available_width - 20.0f); // Minimum 200px, or full width minus padding

        // Get style and position info for mouse-based drag calculation
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec2 window_position = ImGui::GetWindowPos();
        ImVec2 cursor_start_position = ImGui::GetCursorPos();

        // Calculate item height (selectable height + spacing)
        float item_height = ImGui::GetTextLineHeightWithSpacing();

        // Calculate the starting position for the first item
        ImVec2 first_item_position(
            window_position.x + cursor_start_position.x,
            window_position.y + cursor_start_position.y
        );

        for (int n = 0; n < static_cast<int>(ElementInteractionType::kCount); n++) {
            ElementInteractionType element_type = ui_priority_order[n];
            const char* item = ElementInteractionTypeToString(element_type);

            // Create a button-like selectable with some styling
            ImGui::PushID(n);

            // Add some visual styling to make it look more like a button
            ImVec4 button_color = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
            ImVec4 button_hovered = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            ImVec4 button_active = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

            ImGui::PushStyleColor(ImGuiCol_Header, button_color);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, button_hovered);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, button_active);

            // Add priority number and icon with better formatting
            std::string display_text = std::to_string(n + 1) + ". " + std::string(item) + " ⚬";

            // Use explicit width to ensure text is not cut off
            bool is_selected = ImGui::Selectable(display_text.c_str(), false, ImGuiSelectableFlags_None, ImVec2(item_width, item_height));

            ImGui::PopStyleColor(3);
            ImGui::PopID();

            // Handle drag and drop reordering using mouse position
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                // Calculate which item position the mouse is currently over
                float mouse_y = ImGui::GetMousePos().y;
                int target_index = static_cast<int>((mouse_y - first_item_position.y) / item_height);

                // Clamp target index to valid range
                target_index = std::max(0, std::min(target_index, static_cast<int>(ElementInteractionType::kCount) - 1));

                // Only swap if we're targeting a different position
                if (target_index != n && target_index >= 0 && target_index < static_cast<int>(ElementInteractionType::kCount) ) {
                    // Swap the items
                    ElementInteractionType temp = ui_priority_order[n];
                    ui_priority_order[n] = ui_priority_order[target_index];
                    ui_priority_order[target_index] = temp;
                }
            }

            // Show tooltip with more information
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Priority %d: %s", n + 1, item);
                ImGui::TextWrapped("Drag up/down to change priority. Higher priority elements are selected first when multiple elements overlap.");
                ImGui::EndTooltip();
            }
        }

        ImGui::PopItemFlag();

        ImGui::Spacing();
        if (ImGui::Button("Reset to Default Priority")) {
            // Reset to default order
            m_control_settings_->ResetElementPriorityToDefault();
            ui_priority_order = m_control_settings_->GetElementPriorityOrder();
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply Changes")) {
            // Apply the priority changes to the actual system
            m_control_settings_->SetElementPriorityOrder(ui_priority_order);
            std::cout << "Interaction priority order updated:" << std::endl;
            for (int i = 0; i < static_cast<int>(ElementInteractionType::kCount); i++) {
                std::cout << "  " << (i + 1) << ". " << ElementInteractionTypeToString(ui_priority_order[i]) << std::endl;
            }
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Apply the current priority order to element selection behavior.");
        }
		ImGui::Unindent();
    }
}

void SettingsWindow::ShowLayerControls(const std::shared_ptr<Board>& currentBoard)
{  // Updated signature
    if (!currentBoard) {
        ImGui::TextDisabled("No board loaded. Layer controls unavailable.");
        return;
    }

    if (currentBoard->GetLayerCount() == 0) {
        ImGui::TextDisabled("Board has no layers defined.");
        return;
    }

    bool folding_enabled = m_board_data_manager_->IsBoardFoldingEnabled(); // Use current state, not pending
    const auto& layers = currentBoard->GetLayers();

    // "Show All" button
    if (ImGui::Button("Show All Layers")) {
        for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
            if (i < layers.size()) {
                int layer_id = layers[i].GetId();
                // Skip component/pin layers when folding is enabled (they're controlled by board side view)
                if (folding_enabled && (layer_id == Board::kTopCompLayer || layer_id == Board::kBottomCompLayer ||
                                       layer_id == Board::kTopPinsLayer || layer_id == Board::kBottomPinsLayer)) {
                    continue;
                }
            }
            m_board_data_manager_->SetLayerVisible(i, true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide All Layers")) {
        for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
            if (i < layers.size()) {
                int layer_id = layers[i].GetId();
                // Skip component/pin layers when folding is enabled
                if (folding_enabled && (layer_id == Board::kTopCompLayer || layer_id == Board::kBottomCompLayer ||
                                       layer_id == Board::kTopPinsLayer || layer_id == Board::kBottomPinsLayer)) {
                    continue;
                }
            }
            m_board_data_manager_->SetLayerVisible(i, false);
        }
    }

    ImGui::Spacing();

    // Group trace layers (1-16) in a collapsible section
    if (ImGui::CollapsingHeader("Trace Layers (1-16)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        // Toggle all traces button
        if (ImGui::SmallButton("All On##traces")) {
            for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
                if (i < layers.size()) {
                    int layer_id = layers[i].GetId();
                    if (layer_id >= 1 && layer_id <= 16) {
                        m_board_data_manager_->SetLayerVisible(i, true);
                    }
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("All Off##traces")) {
            for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
                if (i < layers.size()) {
                    int layer_id = layers[i].GetId();
                    if (layer_id >= 1 && layer_id <= 16) {
                        m_board_data_manager_->SetLayerVisible(i, false);
                    }
                }
            }
        }

        // Show individual trace layer controls
        for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
            if (i < layers.size()) {
                int layer_id = layers[i].GetId();
                if (layer_id >= 1 && layer_id <= 16) {
                    // Push unique ID to avoid conflicts
                    ImGui::PushID(i);

                    std::string layerName = currentBoard->GetLayerName(i);
                    if (layerName.empty()) {
                        layerName = "Layer " + std::to_string(layer_id);
                    }

                    bool layerVisible = currentBoard->IsLayerVisible(i);
                    if (ImGui::Checkbox(layerName.c_str(), &layerVisible)) {
                        m_board_data_manager_->SetLayerVisible(i, layerVisible);
                    }

                    ImGui::PopID();
                }
            }
        }

        ImGui::Unindent();
    }

    // Group unknown layers (18-27) in a collapsible section
    if (ImGui::CollapsingHeader("Unknown Layers (18-27)")) {
        ImGui::Indent();

        // Toggle all unknown layers button
        if (ImGui::SmallButton("All On##unknown")) {
            for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
                if (i < layers.size()) {
                    int layer_id = layers[i].GetId();
                    if (layer_id >= 18 && layer_id <= 27) {
                        m_board_data_manager_->SetLayerVisible(i, true);
                    }
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("All Off##unknown")) {
            for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
                if (i < layers.size()) {
                    int layer_id = layers[i].GetId();
                    if (layer_id >= 18 && layer_id <= 27) {
                        m_board_data_manager_->SetLayerVisible(i, false);
                    }
                }
            }
        }

        // Show individual unknown layer controls
        for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
            if (i < layers.size()) {
                int layer_id = layers[i].GetId();
                if (layer_id >= 18 && layer_id <= 27) {
                    // Push unique ID to avoid conflicts
                    ImGui::PushID(i);

                    std::string layerName = currentBoard->GetLayerName(i);
                    if (layerName.empty()) {
                        layerName = "Unknown Layer " + std::to_string(layer_id);
                    }

                    bool layerVisible = currentBoard->IsLayerVisible(i);
                    if (ImGui::Checkbox(layerName.c_str(), &layerVisible)) {
                        m_board_data_manager_->SetLayerVisible(i, layerVisible);
                    }

                    ImGui::PopID();
                }
            }
        }

        ImGui::Unindent();
    }

    // Component and Pin layers (always show these controls)
    if (ImGui::CollapsingHeader("Components & Pins", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
            if (i < layers.size()) {
                int layer_id = layers[i].GetId();

                // Only show component and pin layers
                if (layer_id == Board::kTopCompLayer || layer_id == Board::kBottomCompLayer ||
                    layer_id == Board::kTopPinsLayer || layer_id == Board::kBottomPinsLayer) {

                    // Push unique ID to avoid conflicts
                    ImGui::PushID(i);

                    std::string layerName = currentBoard->GetLayerName(i);
					if (layer_id == Board::kTopCompLayer) layerName = "Top Components";
					else if (layer_id == Board::kBottomCompLayer) layerName = "Bottom Components";
					else if (layer_id == Board::kTopPinsLayer) layerName = "Top Pins";
					else if (layer_id == Board::kBottomPinsLayer) layerName = "Bottom Pins";
					else layerName = "Layer " + std::to_string(layer_id);
				

                    bool layerVisible = currentBoard->IsLayerVisible(i);

                    // Disable controls when folding is enabled (but still show current state)
                    if (ImGui::Checkbox(layerName.c_str(), &layerVisible)) {
                        m_board_data_manager_->SetLayerVisible(i, layerVisible);
                    }

                    ImGui::PopID();
                }
            }
        }

        ImGui::Unindent();
    }

    // Other layers (not in the above groups)
    if (ImGui::CollapsingHeader("Other Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        for (int i = 0; i < currentBoard->GetLayerCount(); ++i) {
            if (i < layers.size()) {
                int layer_id = layers[i].GetId();

                // Skip layers that are in other groups or VIAs (too integrated with trace layers)
                if ((layer_id >= 1 && layer_id <= 16) || (layer_id >= 18 && layer_id <= 27) ||
                    layer_id == Board::kTopCompLayer || layer_id == Board::kBottomCompLayer ||
                    layer_id == Board::kTopPinsLayer || layer_id == Board::kBottomPinsLayer ||
                    layer_id == Board::kViasLayer) {
                    continue;
                }

                // Push unique ID to avoid conflicts
                ImGui::PushID(i);

                std::string layerName = currentBoard->GetLayerName(i);
                if (layerName.empty()) {
                    layerName = "Layer " + std::to_string(layer_id);
                }

                bool layerVisible = currentBoard->IsLayerVisible(i);
                if (ImGui::Checkbox(layerName.c_str(), &layerVisible)) {
                    m_board_data_manager_->SetLayerVisible(i, layerVisible);
                }

                ImGui::PopID();
            }
        }

        ImGui::Unindent();
    }
}

void SettingsWindow::ShowAppearanceSettings(const std::shared_ptr<Board>& currentBoard)
{
    // Board View Settings
    ImGui::SeparatorText("Board View");

    // Board Folding Toggle with pending state display
    bool current_folding_enabled = m_board_data_manager_->IsBoardFoldingEnabled();
    bool pending_folding_enabled = m_board_data_manager_->GetPendingBoardFoldingEnabled();
    bool has_pending_change = m_board_data_manager_->HasPendingFoldingChange();

    // Use the pending setting for the checkbox display
    bool checkbox_value = pending_folding_enabled;
    if (ImGui::Checkbox("Enable Board Folding", &checkbox_value)) {
        m_board_data_manager_->SetBoardFoldingEnabled(checkbox_value);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Fold the board to stack components from both sides for easier inspection.\nComponents will be mirrored and assigned to top/bottom mounting sides.");
    }

    // Show pending state information
    if (has_pending_change) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "(pending)");
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.4f, 1.0f),
                          "Board folding: %s → %s (will apply on next board load)",
                          current_folding_enabled ? "enabled" : "disabled",
                          pending_folding_enabled ? "enabled" : "disabled");
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f),
                          "Board folding: %s",
                          current_folding_enabled ? "enabled" : "disabled");
    }

    // Board Side View Selection (only show if current folding is enabled, not pending)
    if (current_folding_enabled) {
        ImGui::Indent();

        BoardDataManager::BoardSide current_side = m_board_data_manager_->GetCurrentViewSide();
        const char* side_options[] = {"Top Side", "Bottom Side"};  // Removed "Both Sides"
        int current_side_index = static_cast<int>(current_side);

        // Ensure current_side_index is valid (0 or 1) since we removed "Both Sides"
        if (current_side_index >= 2) {
            current_side_index = 0;  // Default to Top Side if it was "Both Sides"
            m_board_data_manager_->SetCurrentViewSide(BoardDataManager::BoardSide::kTop);
        }

        // if (ImGui::Combo("Board Side", &current_side_index, side_options, IM_ARRAYSIZE(side_options))) {
        //     m_board_data_manager_->SetCurrentViewSide(static_cast<BoardDataManager::BoardSide>(current_side_index));
        // }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select which side of the board to view.\nUse middle mouse click or F key to quickly flip the board.");
        }

        // Show current view side status with flip indicator
        // bool is_flipped = m_board_data_manager_->IsGlobalHorizontalMirrorEnabled();
        // const char* flip_status = is_flipped ? " (Flipped)" : "";
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Currently viewing: %s%s",
                          current_side == BoardDataManager::BoardSide::kTop ? "Top Side" : "Bottom Side",
                          "");

        // Show board flipping status
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Board flipping: %s",
                          m_board_data_manager_->CanFlipBoard() ? "Enabled" : "Disabled");

        ImGui::Unindent();
    } else {
        // When folding is disabled, show information about board flipping
        ImGui::Indent();
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.7f, 1.0f), "Board flipping disabled");
        ImGui::TextWrapped("Board flipping (F key / middle mouse) is only available when board folding is enabled and viewing Top or Bottom side.");
        ImGui::Unindent();
    }

    // TODO: Move these to their own sections

    if (ImGui::CollapsingHeader("Board Colors")) {
        ImGui::Indent();

        // ========================================
        // HIGHLIGHTING COLORS
        // ========================================
        ImGui::SeparatorText("Highlighting");

        // Selected Element Highlight Color
        RenderColorControl("Selected Element Highlight", BoardDataManager::ColorType::kSelectedElementHighlight,
                          "Color used to highlight directly selected components/pins (separate from net highlighting).");

        // Net Highlight Color
        RenderColorControl("Net Highlight Color", BoardDataManager::ColorType::kNetHighlight,
                          "Color used to highlight elements that belong to the selected net.");

        ImGui::Spacing();

        // ========================================
        // COMPONENT COLORS
        // ========================================
        ImGui::SeparatorText("Components");

        // Component Fill Color
        RenderColorControl("Component Fill Color", BoardDataManager::ColorType::kComponentFill,
                          "Color used to render the background/fill of components.");

        // Component Stroke Color and Thickness
        RenderColorControl("Component Stroke Color", BoardDataManager::ColorType::kComponentStroke,
                          "Color used to render the outline/stroke of components.");

        ImGui::SameLine();
		ImGui::Text("| Thickness");
        ImGui::SameLine();
        float componentStrokeThickness = m_board_data_manager_->GetComponentStrokeThickness();
        ImGui::SetNextItemWidth(80);
        if (ImGui::SliderFloat("##ComponentStrokeThickness", &componentStrokeThickness, 0.01f, 2.0f, "%.2f")) {
            m_board_data_manager_->SetComponentStrokeThickness(componentStrokeThickness);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Thickness of component stroke/outline.");
        }

        ImGui::Spacing();

        // ========================================
        // PIN COLORS
        // ========================================
        ImGui::SeparatorText("Pins");

        // Pin Fill Color
        RenderColorControl("Pin Fill Color", BoardDataManager::ColorType::kPinFill,
                          "Color used to render the background/fill of pins.");

        // Pin Stroke Color and Thickness
        RenderColorControl("Pin Stroke Color", BoardDataManager::ColorType::kPinStroke,
                          "Color used to render the outline/stroke of pins.");

        ImGui::SameLine();
		ImGui::Text("| Thickness");
        ImGui::SameLine();
        float pinStrokeThickness = m_board_data_manager_->GetPinStrokeThickness();
        ImGui::SetNextItemWidth(80);
        if (ImGui::SliderFloat("##PinStrokeThickness", &pinStrokeThickness, 0.01f, 1.0f, "%.2f")) {
            m_board_data_manager_->SetPinStrokeThickness(pinStrokeThickness);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Thickness of pin stroke/outline.");
        }

        // Special Pin Colors
        RenderColorControl("GND Pin Color", BoardDataManager::ColorType::kGND,
                          "Color used to render pins connected to GND nets.");

        RenderColorControl("NC Pin Color", BoardDataManager::ColorType::kNC,
                          "Color used to render pins that are not connected (NC).");

        ImGui::Spacing();

        // ========================================
        // LAYER COLORS
        // ========================================
        ImGui::SeparatorText("Layers");

        // Base Layer Color
        RenderColorControl("Base Layer Color", BoardDataManager::ColorType::kBaseLayer,
                          "The starting color for the first layer. Subsequent layers will have their hue shifted from this color.");
       
        ImGui::Spacing();  // Hue Step per Layer
        float hueStep = m_board_data_manager_->GetLayerHueStep();
        if (ImGui::DragFloat("Hue Shift per Layer", &hueStep, 1.0f, 0.0f, 180.0f, "%.1f degrees")) {
            m_board_data_manager_->SetLayerHueStep(hueStep);
            m_board_data_manager_->RegenerateLayerColors(currentBoard);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("How much the hue is shifted for each subsequent layer, in degrees.");
        }

        ImGui::Spacing();

        // ========================================
        // BOARD APPEARANCE
        // ========================================
        ImGui::SeparatorText("Board Appearance");

        // Silkscreen Color
        RenderColorControl("Silkscreen Color", BoardDataManager::ColorType::kSilkscreen,
                          "Color used to render silkscreen elements.");

        // Board Edges Color and Thickness
        RenderColorControl("Board Edges Color", BoardDataManager::ColorType::kBoardEdges,
                          "Color used to render board edges.");

        ImGui::SameLine();
		ImGui::Text("| Thickness");
        ImGui::SameLine();
        float outlineThickness = m_board_data_manager_->GetBoardOutlineThickness();
        ImGui::SetNextItemWidth(80);
        if (ImGui::SliderFloat("##BoardOutlineThickness", &outlineThickness, 0.01f, 5.0f, "%.2f")) {
            m_board_data_manager_->SetBoardOutlineThickness(outlineThickness);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Thickness of the board outline/edges rendering.");
        }
		ImGui::Unindent();
	}
    ImGui::Spacing();


    ShowLayerControls(currentBoard);  // Pass currentBoard
}

void SettingsWindow::RenderUI(const std::shared_ptr<Board>& currentBoard)
{  // Added currentBoard parameter back
    if (!m_is_open_) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(450, 550), ImGuiCond_FirstUseEver);

    bool window_open = ImGui::Begin(m_window_name.c_str(), &m_is_open_);

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
        if (ImGui::BeginTabItem("Accessibility")) {
            ShowAccessibilitySettings();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();  // This End matches the successful Begin
}

void SettingsWindow::RenderColorControl(const char* label, BoardDataManager::ColorType color_type, const char* tooltip)
{
    BLRgba32 color = m_board_data_manager_->GetColor(color_type);
    float color_array[4] = {
        color.r() / 255.0f,
        color.g() / 255.0f,
        color.b() / 255.0f,
        color.a() / 255.0f
    };

    if (ImGui::ColorEdit4(label, color_array, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_None)) {
        m_board_data_manager_->SetColor(color_type,
                                        BLRgba32(static_cast<uint32_t>(color_array[0] * 255),
                                                static_cast<uint32_t>(color_array[1] * 255),
                                                static_cast<uint32_t>(color_array[2] * 255),
                                                static_cast<uint32_t>(color_array[3] * 255)));
    }

    if (ImGui::IsItemHovered() && tooltip) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

void SettingsWindow::TriggerGridRedraw()
{
    // Use the same callback mechanism as board color controls for consistency
    // This ensures grid color changes trigger the same redraw path as other settings
    if (m_board_data_manager_) {
        // Trigger the settings change callback by calling a BoardDataManager method
        // This is the same mechanism used by RenderColorControl for board colors
        m_board_data_manager_->SetLayerHueStep(m_board_data_manager_->GetLayerHueStep());
    }
}

void SettingsWindow::ShowAccessibilitySettings()
{
    if (ImGui::CollapsingHeader("Font Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        // Font Scale Multiplier
        ImGui::Text("Font Scale Multiplier");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjusts the base font size for better readability.\nChanges take effect immediately.");
        }

        float previous_scale = m_font_scale_multiplier_;
        if (ImGui::SliderFloat("##FontScale", &m_font_scale_multiplier_, 0.5f, 3.0f, "%.1fx")) {
            // Clamp to reasonable bounds
            if (m_font_scale_multiplier_ < 0.5f) m_font_scale_multiplier_ = 0.5f;
            if (m_font_scale_multiplier_ > 3.0f) m_font_scale_multiplier_ = 3.0f;

            // Apply font scaling immediately
            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = m_font_scale_multiplier_;
            m_font_settings_changed_ = true;

            // Grid measurement overlay now uses ImGui and scales automatically
        }

        // Reset button
        ImGui::SameLine();
        if (ImGui::Button("Reset##FontScale")) {
            m_font_scale_multiplier_ = 1.0f;
            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = m_font_scale_multiplier_;
            m_font_settings_changed_ = true;

            // Grid measurement overlay now uses ImGui and scales automatically
        }

        // Show current scale info
        ImGui::Text("Current scale: %.1fx", m_font_scale_multiplier_);

        ImGui::Spacing();
        ImGui::TextWrapped("Note: Font scaling affects all UI elements. Very large scales may cause layout issues.");

        ImGui::Unindent();
    }
}

void SettingsWindow::LoadFontSettingsFromConfig(const Config& config)
{
    // Load font scale multiplier
    m_font_scale_multiplier_ = config.GetFloat("accessibility.font_scale_multiplier", 1.0f);

    // Clamp to safe bounds
    if (m_font_scale_multiplier_ < 0.5f) m_font_scale_multiplier_ = 0.5f;
    if (m_font_scale_multiplier_ > 3.0f) m_font_scale_multiplier_ = 3.0f;

    // Apply the loaded font scale immediately
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = m_font_scale_multiplier_;

    // Grid measurement overlay now uses ImGui and scales automatically
}

void SettingsWindow::SaveFontSettingsToConfig(Config& config) const
{
    // Save font scale multiplier
    config.SetFloat("accessibility.font_scale_multiplier", m_font_scale_multiplier_);
}

// Factory function implementation
std::unique_ptr<SettingsWindow> CreateSettingsWindow(
    std::shared_ptr<GridSettings> grid_settings,
    std::shared_ptr<ControlSettings> control_settings,
    std::shared_ptr<BoardDataManager> board_data_manager,
    float* application_clear_color,
    std::shared_ptr<Grid> grid
) {
    return std::unique_ptr<SettingsWindow>(new SettingsWindow(grid_settings, control_settings, board_data_manager, application_clear_color, grid));
}

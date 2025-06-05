#pragma once

#include <string>  // For m_hoveredElementInfo

#include "imgui.h"

#include "pcb/Board.hpp"
#include "ui/interaction/InteractionTool.hpp"
#include "utils/Vec2.hpp"
// Forward declaration
class ControlSettings;
class BoardDataManager;  // Forward Declaration
class Board;             // Forward needed if we pass Board directly, but DataManager is better

class NavigationTool : public InteractionTool
{
public:
    NavigationTool(std::shared_ptr<Camera> camera, std::shared_ptr<Viewport> viewport, std::shared_ptr<ControlSettings> control_settings, std::shared_ptr<BoardDataManager> board_data_manager);
    ~NavigationTool() override = default;

    void ProcessInput(ImGuiIO& io, bool is_viewport_focused, bool is_viewport_hovered, ImVec2 viewport_top_left, ImVec2 viewport_size) override;

    void OnActivated() override;
    void OnDeactivated() override;

    // Selection specific methods
    [[nodiscard]] int GetSelectedNetId() const;
    void ClearSelection();

    // Configuration for the tool, if any
    // void SetPanSpeed(float speed) { m_panSpeed = speed; }
    // void SetZoomSensitivity(float sensitivity) { m_zoomSensitivity = sensitivity; }

private:
    std::shared_ptr<ControlSettings> m_control_settings_;
    std::shared_ptr<BoardDataManager> m_board_data_manager_;
    // --- New members for hover and selection ---
    std::string m_hovered_element_info_;
    bool m_is_hovering_element_ = false;
    int m_selected_net_id_ = -1;  // -1 indicates no net is selected
    // Potentially store more info about the selected element if needed
    // --- End new members ---

    // Input state specific to navigation, if needed outside of ProcessInput scope
    // e.g., bool m_isPanning;

    // Default pan speed, could be configurable
    // float m_panSpeed = 10.0f;
    // float m_zoomSensitivity = 1.1f;
    // float m_rotationSpeed = 1.0f; // Degrees per pixel dragged or per key press
};

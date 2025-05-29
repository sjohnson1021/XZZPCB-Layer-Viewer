#pragma once

#include "ui/interaction/InteractionTool.hpp"
#include "pcb/Board.hpp"
#include "imgui.h"
#include <string> // For m_hoveredElementInfo
#include "utils/Vec2.hpp"
// Forward declaration
class ControlSettings;
class BoardDataManager; // Forward Declaration
class Board;            // Forward needed if we pass Board directly, but DataManager is better

class NavigationTool : public InteractionTool
{
public:
    NavigationTool(std::shared_ptr<Camera> camera,
                   std::shared_ptr<Viewport> viewport,
                   std::shared_ptr<ControlSettings> controlSettings,
                   std::shared_ptr<BoardDataManager> boardDataManager);
    ~NavigationTool() override = default;

    void ProcessInput(ImGuiIO &io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize) override;

    void OnActivated() override;
    void OnDeactivated() override;

    // Selection specific methods
    int GetSelectedNetId() const;
    void ClearSelection();

    // Configuration for the tool, if any
    // void SetPanSpeed(float speed) { m_panSpeed = speed; }
    // void SetZoomSensitivity(float sensitivity) { m_zoomSensitivity = sensitivity; }

private:
    std::shared_ptr<ControlSettings> m_controlSettings;
    std::shared_ptr<BoardDataManager> m_boardDataManager;
    // --- New members for hover and selection ---
    std::string m_hoveredElementInfo;
    bool m_isHoveringElement = false;
    int m_selectedNetId = -1; // -1 indicates no net is selected
    // Potentially store more info about the selected element if needed
    // --- End new members ---

    // Input state specific to navigation, if needed outside of ProcessInput scope
    // e.g., bool m_isPanning;

    // Default pan speed, could be configurable
    // float m_panSpeed = 10.0f;
    // float m_zoomSensitivity = 1.1f;
    // float m_rotationSpeed = 1.0f; // Degrees per pixel dragged or per key press
};
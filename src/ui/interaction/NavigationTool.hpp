#pragma once

#include "ui/interaction/InteractionTool.hpp"

// Forward declaration
class ControlSettings;

class NavigationTool : public InteractionTool {
public:
    NavigationTool(std::shared_ptr<Camera> camera, 
                   std::shared_ptr<Viewport> viewport,
                   std::shared_ptr<ControlSettings> controlSettings);
    ~NavigationTool() override = default;

    void ProcessInput(ImGuiIO& io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize) override;

    // Configuration for the tool, if any
    // void SetPanSpeed(float speed) { m_panSpeed = speed; }
    // void SetZoomSensitivity(float sensitivity) { m_zoomSensitivity = sensitivity; }

private:
    std::shared_ptr<ControlSettings> m_controlSettings;
    // Input state specific to navigation, if needed outside of ProcessInput scope
    // e.g., bool m_isPanning;

    // Default pan speed, could be configurable
    // float m_panSpeed = 10.0f;
    // float m_zoomSensitivity = 1.1f; 
    // float m_rotationSpeed = 1.0f; // Degrees per pixel dragged or per key press
}; 
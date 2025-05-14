#pragma once

#include "imgui.h" // For ImGuiIO, ImVec2
#include <string>
#include <memory>

// Forward declarations
class Camera;
class Viewport;

class InteractionTool {
public:
    InteractionTool(const std::string& name, std::shared_ptr<Camera> camera, std::shared_ptr<Viewport> viewport)
        : m_name(name), m_camera(camera), m_viewport(viewport) {}
    virtual ~InteractionTool() = default;

    InteractionTool(const InteractionTool&) = delete;
    InteractionTool& operator=(const InteractionTool&) = delete;
    InteractionTool(InteractionTool&&) = delete;
    InteractionTool& operator=(InteractionTool&&) = delete;

    // Called by InteractionManager when this tool is active and input occurs.
    // viewportTopLeft and viewportSize are the screen coordinates of the active render area.
    virtual void ProcessInput(ImGuiIO& io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize) = 0;

    // Optional methods for tool lifecycle
    virtual void OnActivated() {}
    virtual void OnDeactivated() {}
    // virtual void RenderToolUI() {} // For tool-specific options in a panel

    const std::string& GetName() const { return m_name; }

protected:
    std::string m_name;
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<Viewport> m_viewport;
}; 
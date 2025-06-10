#pragma once

#include <memory>
#include <string>

#include "imgui.h"  // For ImGuiIO, ImVec2

// Forward declarations
class Camera;
class Viewport;

class InteractionTool
{
public:
    InteractionTool(const std::string& name, std::shared_ptr<Camera> camera, std::shared_ptr<Viewport> viewport) : m_name_(name), m_camera_(camera), m_viewport_(viewport) {}
    virtual ~InteractionTool() = default;

    InteractionTool(const InteractionTool&) = delete;
    InteractionTool& operator=(const InteractionTool&) = delete;
    InteractionTool(InteractionTool&&) = delete;
    InteractionTool& operator=(InteractionTool&&) = delete;

    // Called by InteractionManager when this tool is active and input occurs.
    // viewportTopLeft and viewportSize are the screen coordinates of the active render area.
    virtual void ProcessInput(ImGuiIO& io, bool is_viewport_focused, bool is_viewport_hovered, ImVec2 viewport_top_left, ImVec2 viewport_size) = 0;

    // Optional methods for tool lifecycle
    virtual void OnActivated() {}
    virtual void OnDeactivated() {}
    // virtual void RenderToolUI() {} // For tool-specific options in a panel

    [[nodiscard]] const std::string& GetName() const { return m_name_; }
    [[nodiscard]] const std::shared_ptr<Camera>& GetCamera() const { return m_camera_; }
    [[nodiscard]] const std::shared_ptr<Viewport>& GetViewport() const { return m_viewport_; }

    // Setters
    void SetName(const std::string& name) { m_name_ = name; }
    void SetCamera(std::shared_ptr<Camera> camera) { m_camera_ = camera; }
    void SetViewport(std::shared_ptr<Viewport> viewport) { m_viewport_ = viewport; }

private:
    std::string m_name_;
    std::shared_ptr<Camera> m_camera_;      // Renamed from m_camera
    std::shared_ptr<Viewport> m_viewport_;  // Renamed from m_viewport
};

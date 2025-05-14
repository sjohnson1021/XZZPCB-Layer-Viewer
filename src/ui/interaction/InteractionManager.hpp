#pragma once

#include <memory>
// #include <vector> // Keep for later when managing multiple tools
#include <string>
#include "imgui.h" // For ImGuiIO

// Forward declarations
class Camera;
class Viewport;
// class InteractionTool; // Base class, NavigationTool.hpp will include it.
class NavigationTool; // Concrete tool

class InteractionManager {
public:
    InteractionManager(std::shared_ptr<Camera> camera, std::shared_ptr<Viewport> viewport);
    ~InteractionManager();

    InteractionManager(const InteractionManager&) = delete;
    InteractionManager& operator=(const InteractionManager&) = delete;
    InteractionManager(InteractionManager&&) = delete;
    InteractionManager& operator=(InteractionManager&&) = delete;

    // Called every frame to process inputs
    // Needs context: which window/area is active, mouse position relative to it.
    void ProcessInput(ImGuiIO& io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize);

    // Tool management
    // void AddTool(std::shared_ptr<InteractionTool> tool);
    // bool SetActiveTool(const std::string& toolName);
    // InteractionTool* GetActiveTool() const;

    // For now, let's assume NavigationTool is implicitly active or directly managed.
    // Later, we can add a more generic tool system.
    // If we create NavigationTool directly, it might be a member.
    // std::unique_ptr<NavigationTool> m_navigationTool; // Example

private:
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<Viewport> m_viewport; // Viewport of the render area

    // std::vector<std::shared_ptr<InteractionTool>> m_tools;
    // InteractionTool* m_activeTool = nullptr;

    // Input state that might be relevant for multiple tools or manager itself
    // ImVec2 m_mousePosInViewport; // Calculated in ProcessInput

    // For now, directly own the NavigationTool.
    std::unique_ptr<NavigationTool> m_navigationTool;
}; 
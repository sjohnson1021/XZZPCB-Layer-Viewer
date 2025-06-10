#pragma once

#include <memory>
#include <string>
#include <vector>

#include "imgui.h"  // For ImGuiIO

// Forward declarations
class Camera;
class Viewport;
class InteractionTool;  // Base class
class NavigationTool;   // Concrete tool
// class InspectionTool;  // REMOVED
class ControlSettings;
class BoardDataManager;
class PcbRenderer;

class InteractionManager
{
public:
    InteractionManager(std::shared_ptr<Camera> camera, std::shared_ptr<Viewport> viewport, std::shared_ptr<ControlSettings> control_settings, std::shared_ptr<BoardDataManager> board_data_manager);
    ~InteractionManager();

    InteractionManager(const InteractionManager&) = delete;
    InteractionManager& operator=(const InteractionManager&) = delete;
    InteractionManager(InteractionManager&&) = delete;
    InteractionManager& operator=(InteractionManager&&) = delete;

    // Called every frame to process inputs
    // Needs context: which window/area is active, mouse position relative to it.
    void ProcessInput(ImGuiIO& io, bool is_viewport_focused, bool is_viewport_hovered, ImVec2 viewport_top_left, ImVec2 viewport_size, PcbRenderer* pcb_renderer);

    // Getters

    [[nodiscard]] const std::shared_ptr<Camera>& GetCamera() const;
    [[nodiscard]] const std::shared_ptr<Viewport>& GetViewport() const;
    [[nodiscard]] const std::shared_ptr<ControlSettings>& GetControlSettings() const;
    [[nodiscard]] const std::shared_ptr<BoardDataManager>& GetBoardDataManager() const;

    // Tool management
    void AddTool(std::shared_ptr<InteractionTool> tool);
    bool SetActiveTool(const std::string& tool_name);
    [[nodiscard]] InteractionTool* GetActiveTool() const;
    [[nodiscard]] const std::vector<std::shared_ptr<InteractionTool>>& GetTools() const;

    // For now, let's assume NavigationTool is implicitly active or directly managed.
    // Later, we can add a more generic tool system.
    // If we create NavigationTool directly, it might be a member.
    // std::unique_ptr<NavigationTool> m_navigationTool; // Example

private:
    std::shared_ptr<Camera> m_camera_;
    std::shared_ptr<Viewport> m_viewport_;                    // Viewport of the render area
    std::shared_ptr<ControlSettings> m_control_settings_;     // Added member
    std::shared_ptr<BoardDataManager> m_board_data_manager_;  // Added member

    std::vector<std::shared_ptr<InteractionTool>> m_tools_;
    InteractionTool* m_active_tool_ = nullptr;

    // Input state that might be relevant for multiple tools or manager itself
    // ImVec2 m_mousePosInViewport; // Calculated in ProcessInput

    // For now, directly own the NavigationTool.
    // std::unique_ptr<NavigationTool> m_navigationTool; // Replaced by m_tools
};

#include "ui/interaction/InteractionManager.hpp"
#include "ui/interaction/NavigationTool.hpp" // Include concrete tool
#include "view/Camera.hpp"
#include "view/Viewport.hpp"

InteractionManager::InteractionManager(std::shared_ptr<Camera> camera, std::shared_ptr<Viewport> viewport)
    : m_camera(camera), m_viewport(viewport) {
    // Create and set the navigation tool as the default active tool.
    m_navigationTool = std::make_unique<NavigationTool>(m_camera, m_viewport);
    // m_activeTool = m_navigationTool.get(); // If we use a generic InteractionTool* for m_activeTool
}

InteractionManager::~InteractionManager() {
    // m_navigationTool unique_ptr will auto-delete
}

void InteractionManager::ProcessInput(ImGuiIO& io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize) {
    // Delegate to the active tool, which is m_navigationTool for now.
    if (m_navigationTool) {
        m_navigationTool->ProcessInput(io, isViewportFocused, isViewportHovered, viewportTopLeft, viewportSize);
    } 
    // Later, if using m_activeTool:
    // if (m_activeTool) {
    //     m_activeTool->ProcessInput(io, isViewportFocused, isViewportHovered, viewportTopLeft, viewportSize);
    // }
}

// Future tool management methods:
// void InteractionManager::AddTool(std::shared_ptr<InteractionTool> tool) {
//     m_tools.push_back(tool);
//     if (!m_activeTool) { // Activate the first tool added by default
//         m_activeTool = tool.get();
//         if (m_activeTool) m_activeTool->OnActivated();
//     }
// }

// bool InteractionManager::SetActiveTool(const std::string& toolName) {
//     for (const auto& tool : m_tools) {
//         if (tool->GetName() == toolName) {
//             if (m_activeTool != tool.get()) {
//                 if (m_activeTool) m_activeTool->OnDeactivated();
//                 m_activeTool = tool.get();
//                 if (m_activeTool) m_activeTool->OnActivated();
//             }
//             return true;
//         }
//     }
//     return false;
// }

// InteractionTool* InteractionManager::GetActiveTool() const {
//     return m_activeTool;
// } 
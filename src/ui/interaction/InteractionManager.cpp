#include "InteractionManager.hpp"

#include <iostream>

#include "InteractionTool.hpp"
#include "NavigationTool.hpp"

#include "core/BoardDataManager.hpp"
#include "core/ControlSettings.hpp"
#include "render/PcbRenderer.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"

InteractionManager::InteractionManager(std::shared_ptr<Camera> camera,
                                       std::shared_ptr<Viewport> viewport,
                                       std::shared_ptr<ControlSettings> controlSettings,
                                       std::shared_ptr<BoardDataManager> boardDataManager)
    : m_camera_(camera), m_viewport_(viewport), m_control_settings_(controlSettings), m_board_data_manager_(boardDataManager), m_active_tool_(nullptr)
{
    auto navigationTool = std::make_shared<NavigationTool>(m_camera_, m_viewport_, m_control_settings_, m_board_data_manager_);
    AddTool(navigationTool);

    // Ensure an active tool is set if any tools were added.
    if (!m_tools_.empty() && !m_active_tool_) {
        // This condition should ideally not be met if AddTool correctly activates the first tool.
        // However, as a fallback or explicit confirmation:
        SetActiveTool(m_tools_[0]->GetName());
    } else if (m_tools_.empty()) {
        // This case should ideally not happen if NavigationTool is always added.
        std::cerr << "InteractionManager Error: No tools were added during construction!" << std::endl;
    }
}

InteractionManager::~InteractionManager()
{
    if (m_active_tool_) {
        m_active_tool_->OnDeactivated();
    }
    m_tools_.clear();
}

void InteractionManager::ProcessInput(ImGuiIO& io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize, PcbRenderer* pcbRenderer)
{
    if (m_active_tool_) {
        m_active_tool_->ProcessInput(io, isViewportFocused, isViewportHovered, viewportTopLeft, viewportSize);
    }

    if (GetCamera() && GetCamera()->WasViewChangedThisFrame()) {
        if (pcbRenderer) {
            pcbRenderer->MarkGridDirty();
            pcbRenderer->MarkBoardDirty();
        }
        GetCamera()->ClearViewChangedFlag();
    }
}

void InteractionManager::AddTool(std::shared_ptr<InteractionTool> tool)
{
    if (tool) {
        m_tools_.push_back(std::move(tool));
        if (!m_active_tool_)  // Activate the first tool added by default
        {
            m_active_tool_ = tool.get();
            if (m_active_tool_)  // Call OnActivated only if successfully set.
            {
                std::cout << "InteractionManager: Activating initial tool: " << m_active_tool_->GetName() << std::endl;
                m_active_tool_->OnActivated();
            }
        }
    }
}

bool InteractionManager::SetActiveTool(const std::string& toolName)
{
    auto it = std::find_if(m_tools_.begin(), m_tools_.end(), [&toolName](const std::shared_ptr<InteractionTool>& tool) { return tool->GetName() == toolName; });

    if (it != m_tools_.end()) {
        InteractionTool* newTool = it->get();
        if (m_active_tool_ != newTool)  // Only switch if it's a different tool
        {
            if (m_active_tool_) {
                std::cout << "InteractionManager: Deactivating tool: " << m_active_tool_->GetName() << std::endl;
                m_active_tool_->OnDeactivated();
            }
            m_active_tool_ = newTool;
            if (m_active_tool_)  // Call OnActivated only if successfully set
            {
                std::cout << "InteractionManager: Activating tool: " << m_active_tool_->GetName() << std::endl;
                m_active_tool_->OnActivated();
            }
        }
        return true;
    }
    std::cerr << "InteractionManager Warning: Tool with name '" << toolName << "' not found." << std::endl;
    return false;
}

const std::shared_ptr<Camera>& InteractionManager::GetCamera() const
{
    return m_camera_;
}

const std::shared_ptr<Viewport>& InteractionManager::GetViewport() const
{
    return m_viewport_;
}

const std::shared_ptr<ControlSettings>& InteractionManager::GetControlSettings() const
{
    return m_control_settings_;
}

const std::shared_ptr<BoardDataManager>& InteractionManager::GetBoardDataManager() const
{
    return m_board_data_manager_;
}

InteractionTool* InteractionManager::GetActiveTool() const
{
    return m_active_tool_;
}

const std::vector<std::shared_ptr<InteractionTool>>& InteractionManager::GetTools() const
{
    return m_tools_;
}

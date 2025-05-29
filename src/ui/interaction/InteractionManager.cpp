#include "ui/interaction/InteractionManager.hpp"
#include "ui/interaction/NavigationTool.hpp"
#include "ui/interaction/InteractionTool.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "core/ControlSettings.hpp"
#include "core/BoardDataManager.hpp"
#include "render/PcbRenderer.hpp"
#include <iostream>

InteractionManager::InteractionManager(std::shared_ptr<Camera> camera,
                                       std::shared_ptr<Viewport> viewport,
                                       std::shared_ptr<ControlSettings> controlSettings,
                                       std::shared_ptr<BoardDataManager> boardDataManager)
    : m_camera(camera),
      m_viewport(viewport),
      m_controlSettings(controlSettings),
      m_boardDataManager(boardDataManager),
      m_activeTool(nullptr)
{
    auto navigationTool = std::make_shared<NavigationTool>(m_camera, m_viewport, m_controlSettings, m_boardDataManager);
    AddTool(navigationTool);

    // Ensure an active tool is set if any tools were added.
    if (!m_tools.empty() && !m_activeTool)
    {
        // This condition should ideally not be met if AddTool correctly activates the first tool.
        // However, as a fallback or explicit confirmation:
        SetActiveTool(m_tools[0]->GetName());
    }
    else if (m_tools.empty())
    {
        // This case should ideally not happen if NavigationTool is always added.
        std::cerr << "InteractionManager Error: No tools were added during construction!" << std::endl;
    }
}

InteractionManager::~InteractionManager()
{
    if (m_activeTool)
    {
        m_activeTool->OnDeactivated();
    }
    m_tools.clear();
}

void InteractionManager::ProcessInput(ImGuiIO &io, bool isViewportFocused, bool isViewportHovered, ImVec2 viewportTopLeft, ImVec2 viewportSize, PcbRenderer *pcbRenderer)
{
    if (m_activeTool)
    {
        m_activeTool->ProcessInput(io, isViewportFocused, isViewportHovered, viewportTopLeft, viewportSize);
    }

    if (m_camera && m_camera->WasViewChangedThisFrame())
    {
        if (pcbRenderer)
        {
            pcbRenderer->MarkGridDirty();
            pcbRenderer->MarkBoardDirty();
        }
        m_camera->ClearViewChangedFlag();
    }
}

void InteractionManager::AddTool(std::shared_ptr<InteractionTool> tool)
{
    if (tool)
    {
        m_tools.push_back(tool);
        if (!m_activeTool) // Activate the first tool added by default
        {
            m_activeTool = tool.get();
            if (m_activeTool) // Call OnActivated only if successfully set
            {
                std::cout << "InteractionManager: Activating initial tool: " << m_activeTool->GetName() << std::endl;
                m_activeTool->OnActivated();
            }
        }
    }
}

bool InteractionManager::SetActiveTool(const std::string &toolName)
{
    auto it = std::find_if(m_tools.begin(), m_tools.end(),
                           [&toolName](const std::shared_ptr<InteractionTool> &tool)
                           {
                               return tool->GetName() == toolName;
                           });

    if (it != m_tools.end())
    {
        InteractionTool *newTool = it->get();
        if (m_activeTool != newTool) // Only switch if it's a different tool
        {
            if (m_activeTool)
            {
                std::cout << "InteractionManager: Deactivating tool: " << m_activeTool->GetName() << std::endl;
                m_activeTool->OnDeactivated();
            }
            m_activeTool = newTool;
            if (m_activeTool) // Call OnActivated only if successfully set
            {
                std::cout << "InteractionManager: Activating tool: " << m_activeTool->GetName() << std::endl;
                m_activeTool->OnActivated();
            }
        }
        return true;
    }
    std::cerr << "InteractionManager Warning: Tool with name '" << toolName << "' not found." << std::endl;
    return false;
}

InteractionTool *InteractionManager::GetActiveTool() const
{
    return m_activeTool;
}

const std::vector<std::shared_ptr<InteractionTool>> &InteractionManager::GetTools() const
{
    return m_tools;
}
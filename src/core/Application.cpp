#include "Application.hpp"
#include "Config.hpp"
#include "Events.hpp"
#include "Renderer.hpp"
#include "SDLRenderer.hpp"
#include "ImGuiManager.hpp"
#include <imgui.h>
#include <iostream>

Application::Application()
    : m_isRunning(false)
    , m_appName("PCB Viewer")
    , m_windowWidth(1280)
    , m_windowHeight(720)
    , m_showDemoWindow(false)
    , m_clearColor{0.2f, 0.2f, 0.2f, 1.0f}
    , m_showLayerControls(true)
{
}

Application::~Application()
{
    Shutdown();
}

bool Application::Initialize(int argc, char* argv[])
{
    std::cout << "Initializing " << m_appName << "..." << std::endl;
    
    // Create subsystems
    m_config = std::make_unique<Config>();
    m_events = std::make_unique<Events>();
    
    // Parse command line arguments
    if (argc > 1) {
        // TODO: Process command line arguments
    }
    
    // Initialize subsystems
    if (!InitializeSubsystems()) {
        std::cerr << "Failed to initialize subsystems" << std::endl;
        return false;
    }
    
    m_isRunning = true;
    return true;
}

int Application::Run()
{
    std::cout << "Running application..." << std::endl;
    
    while (IsRunning()) {
        ProcessEvents();
        
        // Check if window is minimized
        SDLRenderer* sdlRenderer = dynamic_cast<SDLRenderer*>(m_renderer.get());
        if (sdlRenderer && (SDL_GetWindowFlags(sdlRenderer->GetWindow()) & SDL_WINDOW_MINIMIZED)) {
            SDL_Delay(10);
            continue;
        }
        
        Update();
        Render();
    }
    
    return 0;
}

void Application::Shutdown()
{
    std::cout << "Shutting down..." << std::endl;
    
    // Destroy subsystems in reverse order
    m_imguiManager.reset();
    m_renderer.reset();
    m_events.reset();
    m_config.reset();
}

bool Application::IsRunning() const
{
    return m_isRunning;
}

void Application::Quit()
{
    m_isRunning = false;
}

Config* Application::GetConfig() const
{
    return m_config.get();
}

Events* Application::GetEvents() const
{
    return m_events.get();
}

Renderer* Application::GetRenderer() const
{
    return m_renderer.get();
}

ImGuiManager* Application::GetImGuiManager() const
{
    return m_imguiManager.get();
}

bool Application::InitializeSubsystems()
{
    // Get configuration values
    m_appName = m_config->GetString("application.name", "PCB Viewer");
    m_windowWidth = m_config->GetInt("window.width", 1280);
    m_windowHeight = m_config->GetInt("window.height", 720);
    
    // Initialize renderer
    m_renderer = std::unique_ptr<Renderer>(Renderer::Create());
    if (!m_renderer->Initialize(m_appName, m_windowWidth, m_windowHeight)) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }
    
    // Initialize ImGui
    m_imguiManager = std::make_unique<ImGuiManager>(m_renderer.get());
    if (!m_imguiManager->Initialize()) {
        std::cerr << "Failed to initialize ImGui" << std::endl;
        return false;
    }

    // Connect ImGui to Events system
    m_events->SetImGuiManager(m_imguiManager.get());
    
    return true;
}

void Application::ProcessEvents()
{
    m_events->ProcessEvents();
    
    // Example of handling events
    if (m_events->ShouldQuit()) {
        Quit();
    }
}

void Application::Update()
{
    // Update ImGui
    m_imguiManager->NewFrame();
    
    // Create main dockspace
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(2);
    
    // DockSpace
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));
    
    // Main Menu Bar
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open PCB...", "Ctrl+O")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) { Quit(); }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Demo Window", nullptr, &m_showDemoWindow);
            ImGui::MenuItem("Layer Controls", nullptr, &m_showLayerControls);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About...")) {}
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
    
    ImGui::End(); // DockSpace
    
    // Show demo window if enabled
    if (m_showDemoWindow)
    {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
    
    // Create a PCB View window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("PCB View", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // Get the content area size and position
    m_contentAreaPos = ImGui::GetCursorScreenPos();
    m_contentAreaSize = ImGui::GetContentRegionAvail();
    
    // Add a dummy element that fills the entire window
    ImGui::InvisibleButton("pcb_canvas", m_contentAreaSize);
    
    // Handle mouse interactions in the PCB view
    if (ImGui::IsItemHovered())
    {
        // Mouse wheel for zooming
        if (ImGui::GetIO().MouseWheel != 0)
        {
            // TODO: Implement zoom logic
        }
    }
    
    ImGui::End(); // PCB View
    ImGui::PopStyleVar();
    
    // Layer Controls window
    if (m_showLayerControls)
    {
        ImGui::Begin("Layer Controls", &m_showLayerControls);
        ImGui::Text("PCB Layers");
        ImGui::Separator();
        
        // Example layer toggles
        static bool top_copper = true;
        static bool bottom_copper = true;
        static bool top_silkscreen = true;
        static bool bottom_silkscreen = true;
        
        ImGui::Checkbox("Top Copper", &top_copper);
        ImGui::Checkbox("Bottom Copper", &bottom_copper);
        ImGui::Checkbox("Top Silkscreen", &top_silkscreen);
        ImGui::Checkbox("Bottom Silkscreen", &bottom_silkscreen);
        
        ImGui::Separator();
        ImGui::ColorEdit3("Background", m_clearColor);
        
        ImGui::End();
    }
}

void Application::Render()
{
    // Get the SDLRenderer to access SDL-specific functions
    SDLRenderer* sdlRenderer = dynamic_cast<SDLRenderer*>(m_renderer.get());
    if (!sdlRenderer) return;
    
    // Clear the entire screen with content area color
    SDL_SetRenderDrawColorFloat(sdlRenderer->GetRenderer(), 
        m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
    SDL_RenderClear(sdlRenderer->GetRenderer());
    
    // Draw in the PCB View window
    if (m_contentAreaSize.x > 0 && m_contentAreaSize.y > 0)
    {
        // Draw a grid pattern in the PCB view area
        SDL_FRect gridRect = { 
            m_contentAreaPos.x, 
            m_contentAreaPos.y, 
            m_contentAreaSize.x, 
            m_contentAreaSize.y 
        };
        
        // Draw grid lines
        SDL_SetRenderDrawColorFloat(sdlRenderer->GetRenderer(), 0.4f, 0.4f, 0.4f, 1.0f);
        
        float gridSize = 20.0f;
        for (float x = gridRect.x; x < gridRect.x + gridRect.w; x += gridSize)
        {
            SDL_FRect lineRect = {x, gridRect.y, 1.0f, gridRect.h};
            SDL_RenderFillRect(sdlRenderer->GetRenderer(), &lineRect);
        }
        
        for (float y = gridRect.y; y < gridRect.y + gridRect.h; y += gridSize)
        {
            SDL_FRect lineRect = {gridRect.x, y, gridRect.w, 1.0f};
            SDL_RenderFillRect(sdlRenderer->GetRenderer(), &lineRect);
        }
        
        // Draw a placeholder PCB outline
        SDL_SetRenderDrawColorFloat(sdlRenderer->GetRenderer(), 0.0f, 0.8f, 0.0f, 1.0f);
        SDL_FRect pcbRect = {
            m_contentAreaPos.x + m_contentAreaSize.x * 0.2f,
            m_contentAreaPos.y + m_contentAreaSize.y * 0.2f,
            m_contentAreaSize.x * 0.6f,
            m_contentAreaSize.y * 0.6f
        };
        SDL_RenderRect(sdlRenderer->GetRenderer(), &pcbRect);
    }
    
    // Render ImGui on top
    m_imguiManager->Render();
    
    m_renderer->Present();
} 
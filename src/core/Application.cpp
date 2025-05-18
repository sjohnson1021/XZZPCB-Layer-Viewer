#include "Application.hpp"
#include "Config.hpp"
#include "Events.hpp"
#include "Renderer.hpp"
#include "SDLRenderer.hpp"
#include "ImGuiManager.hpp"
#include "ui/MainMenuBar.hpp"
#include "ui/windows/PCBViewerWindow.hpp"
#include "ui/windows/SettingsWindow.hpp"
#include "ui/windows/PcbDetailsWindow.hpp"
#include "core/ControlSettings.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "view/Grid.hpp"
#include "view/GridSettings.hpp"
#include "pcb/Board.hpp"
#include "pcb/PcbLoader.hpp"
#include "utils/StringUtils.hpp"
#include "utils/OpenGLUtils.hpp"
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <SDL3/SDL_filesystem.h>
#include <filesystem>
#include <iostream>

namespace {
    std::string GetAppConfigFilePath() {
        const char* CONFIG_FILENAME = "XZZPCBViewer_settings.ini";
        char* prefPath_c = SDL_GetPrefPath("sjohnson1021", "XZZPCBViewer");
        if (!prefPath_c) {
            std::cerr << "Warning: SDL_GetPrefPath failed. Using current directory for config." << std::endl;
            return std::filesystem::current_path().append(CONFIG_FILENAME).string();
        }
        std::filesystem::path pathObj(prefPath_c);
        SDL_free(prefPath_c);
        
        if (!std::filesystem::exists(pathObj)) {
            try {
                if (!std::filesystem::create_directories(pathObj)) {
                    if (!std::filesystem::exists(pathObj)) {
                        std::cerr << "Error: Could not create preference directory: " << pathObj.string() 
                                  << ". Falling back to current directory." << std::endl;
                        return std::filesystem::current_path().append(CONFIG_FILENAME).string();
                    }
                }
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Error creating directory " << pathObj.string() << ": " << e.what() 
                          << ". Falling back to current directory." << std::endl;
                return std::filesystem::current_path().append(CONFIG_FILENAME).string();
            }
        }
        pathObj /= CONFIG_FILENAME;
        return pathObj.string();
    }
}

Application::Application()
    : m_isRunning(false)
    , m_appName("XZZPCB Layer Viewer")
    , m_windowWidth(1280)
    , m_windowHeight(720)
    , m_clearColor{0.1f, 0.105f, 0.11f, 1.0f}
    , m_showPcbLoadErrorModal(false)
    , m_pcbLoadErrorMessage("")
{
}

Application::~Application()
{
    Shutdown();
}

void Application::LoadConfig() {
    m_config = std::make_unique<Config>();
    std::string configFilePath = GetAppConfigFilePath(); 
    if (m_config->LoadFromFile(configFilePath)) {
        std::cout << "Successfully loaded config from " << configFilePath << std::endl;
    } else {
        std::cout << "Config file " << configFilePath << " not found or failed to load. Using defaults." << std::endl;
    }

    m_appName = m_config->GetString("application.name", m_appName);
    m_windowWidth = m_config->GetInt("window.width", m_windowWidth);
    m_windowHeight = m_config->GetInt("window.height", m_windowHeight);

    m_controlSettings = std::make_shared<ControlSettings>();
    m_controlSettings->LoadKeybindsFromConfig(*m_config);
}

bool Application::InitializeCoreSubsystems()
{
    m_events = std::make_unique<Events>();

    m_renderer = std::unique_ptr<Renderer>(Renderer::Create());
    if (!m_renderer || !m_renderer->Initialize(m_appName, m_windowWidth, m_windowHeight)) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }
    
    m_imguiManager = std::make_unique<ImGuiManager>(m_renderer.get());
    if (!m_imguiManager || !m_imguiManager->Initialize()) {
        std::cerr << "Failed to initialize ImGuiManager" << std::endl;
        return false;
    }

    std::string imguiIniDataKey = "imgui.ini_data";
    std::string imguiIniDataEscaped = m_config->GetString(imguiIniDataKey);
    if (!imguiIniDataEscaped.empty()) {
        std::string imguiIniDataUnescaped = StringUtils::UnescapeNewlines(imguiIniDataEscaped);
        ImGui::LoadIniSettingsFromMemory(imguiIniDataUnescaped.c_str(), imguiIniDataUnescaped.length());
    }
    ImGui::GetIO().IniFilename = nullptr;

    m_events->SetImGuiManager(m_imguiManager.get());
    
    return true;
}

bool Application::InitializeUISubsystems() {
    m_camera = std::make_shared<Camera>();
    m_viewport = std::make_shared<Viewport>();
    m_gridSettings = std::make_shared<GridSettings>();
    m_grid = std::make_shared<Grid>(m_gridSettings);

    m_mainMenuBar = std::make_unique<MainMenuBar>();
    m_pcbViewerWindow = std::make_unique<PCBViewerWindow>(m_camera, m_viewport, m_grid, m_gridSettings, m_controlSettings);
    m_settingsWindow = std::make_unique<SettingsWindow>(m_gridSettings, m_controlSettings, m_clearColor);
    m_pcbDetailsWindow = std::make_unique<PcbDetailsWindow>();

    m_fileDialogInstance = std::make_unique<ImGuiFileDialog>();

    m_settingsWindow->SetVisible(true);
    m_pcbDetailsWindow->SetVisible(false);

    return true;
}

bool Application::Initialize()
{
    std::cout << "Initializing " << m_appName << "..." << std::endl;
    
    LoadConfig();
    
    if (!InitializeCoreSubsystems()) {
        std::cerr << "Failed to initialize core subsystems" << std::endl;
        return false;
    }

    if (!InitializeUISubsystems()) {
        std::cerr << "Failed to initialize UI subsystems" << std::endl;
        return false;
    }
    
    m_isRunning = true;
    return true;
}

int Application::Run()
{
    std::cout << "Running application..." << std::endl;
    
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (IsRunning()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
        lastTime = currentTime;

        ProcessEvents();
        
        Update(deltaTime);
        Render();
    }
    
    Shutdown(); 
    return 0;
}

void Application::Shutdown()
{
    std::cout << "Shutting down..." << std::endl;

    std::string configFilePath = GetAppConfigFilePath();

    if (m_config && ImGui::GetCurrentContext()) {
        size_t imguiIniDataSize = 0;
        const char* imguiIniContent = ImGui::SaveIniSettingsToMemory(&imguiIniDataSize);
        std::string imguiIniDataKey = "imgui.ini_data";
        if (imguiIniContent && imguiIniDataSize > 0) {
            m_config->SetString(imguiIniDataKey, std::string(imguiIniContent, imguiIniDataSize));
        } else {
            m_config->SetString(imguiIniDataKey, ""); 
        }
    }

    if (m_controlSettings && m_config) {
        m_controlSettings->SaveKeybindsToConfig(*m_config);
    }

    if (m_config) {
        if (!m_config->SaveToFile(configFilePath)) {
            std::cerr << "Error: Failed to save config file to " << configFilePath << std::endl;
        }
    }

    m_pcbDetailsWindow.reset();
    m_settingsWindow.reset();
    m_pcbViewerWindow.reset();
    m_mainMenuBar.reset();
    m_fileDialogInstance.reset();

    m_imguiManager.reset();
    m_renderer.reset();
    m_events.reset();
    m_config.reset();

    m_currentBoard.reset();
    m_grid.reset();
    m_gridSettings.reset();
    m_viewport.reset();
    m_camera.reset();
    m_controlSettings.reset();

    m_isRunning = false;
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

void Application::ProcessEvents()
{
    m_events->ProcessEvents();
    
    if (m_events->ShouldQuit()) {
        Quit();
    }
}

void Application::Update(float deltaTime)
{
    // (void)deltaTime;
}

void Application::RenderUI() {
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    if (m_mainMenuBar) {
        m_mainMenuBar->RenderUI(*this);
    }

    if (m_quitFileRequested) {
        Quit();
        m_quitFileRequested = false;
    }

    if (m_openFileRequested) {
        if (m_fileDialogInstance) {
             IGFD::FileDialogConfig config;
             config.path = ".";
             // Consider adding flags if specific behavior (e.g., modal) is desired:
             // config.flags = ImGuiFileDialogFlags_Modal; 
             m_fileDialogInstance->OpenDialog("ChooseFileDlgKey", "Choose File", ".kicad_pcb,.pcb", config);
        }
        m_openFileRequested = false;
    }

    if (m_settingsWindow && m_showSettingsRequested) {
        m_settingsWindow->SetVisible(true);
        m_showSettingsRequested = false;
    }

    if (m_pcbDetailsWindow && m_showPcbDetailsRequested) {
        m_pcbDetailsWindow->SetVisible(true);
        m_showPcbDetailsRequested = false;
    }

    if (m_fileDialogInstance && m_fileDialogInstance->Display("ChooseFileDlgKey")) {
        if (m_fileDialogInstance->IsOk()) {
            std::string filePathName = m_fileDialogInstance->GetFilePathName();
            OpenPcbFile(filePathName);
        }
        m_fileDialogInstance->Close();
    }

    if (m_pcbViewerWindow && m_renderer) {
        SDL_Renderer* sdlRenderer = nullptr;
        if (auto* concreteRenderer = dynamic_cast<SDLRenderer*>(m_renderer.get())) {
            sdlRenderer = concreteRenderer->GetRenderer();
        } else {
            void* rendererHandle = m_renderer->GetRendererHandle();
            if (rendererHandle) {
                sdlRenderer = static_cast<SDL_Renderer*>(rendererHandle);
            }
        }
        if (sdlRenderer) {
            m_pcbViewerWindow->RenderUI(sdlRenderer);
        }
    }

    // Temporarily disable SettingsWindow and PcbDetailsWindow for diagnostics
    if (m_settingsWindow) {
        m_settingsWindow->RenderUI(m_currentBoard);
    }

    if (m_showPcbLoadErrorModal) {
        ImGui::OpenPopup("PCB Load Error");
        m_showPcbLoadErrorModal = false;
    }
    if (ImGui::BeginPopupModal("PCB Load Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", m_pcbLoadErrorMessage.c_str());
        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Application::Render()
{
    GL_CHECK_ERRORS();

    m_imguiManager->NewFrame();
    
    RenderUI();

    GL_CHECK(m_renderer->Clear());
    
    GL_CHECK(m_imguiManager->Render()); 
    
    GL_CHECK(m_renderer->Present());

    GL_CHECK_ERRORS();
}

void Application::OpenPcbFile(const std::string& filePath) {
    std::cout << "Attempting to open PCB file: " << filePath << std::endl;

    auto newBoard = std::make_shared<Board>(filePath);

    if (newBoard && newBoard->IsLoaded()) {
        m_currentBoard = newBoard;
        std::cout << "Successfully loaded PCB: " << filePath << std::endl;
        if (m_pcbViewerWindow) {
            m_pcbViewerWindow->OnBoardLoaded(m_currentBoard);
        }
        if (m_pcbDetailsWindow) {
            m_pcbDetailsWindow->setBoard(m_currentBoard);
        }
        m_showPcbLoadErrorModal = false;
    } else {
        std::cerr << "Failed to load PCB: " << filePath << ". Error: " << (newBoard ? newBoard->GetErrorMessage() : "Unknown error") << std::endl;
        m_currentBoard = nullptr;
        m_pcbLoadErrorMessage = "Failed to load board: " + filePath + "\n";
        if (newBoard) {
            m_pcbLoadErrorMessage += newBoard->GetErrorMessage();
        }
        m_showPcbLoadErrorModal = true;
        if (m_pcbViewerWindow) {
             m_pcbViewerWindow->OnBoardLoaded(nullptr);
        }
        if (m_pcbDetailsWindow) {
            m_pcbDetailsWindow->setBoard(nullptr);
        }
    }
} 
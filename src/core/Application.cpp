#include "Application.hpp"

#include <filesystem>
#include <iostream>

#include <SDL3/SDL_filesystem.h>

#include "Config.hpp"
#include "Events.hpp"
#include "ImGuiManager.hpp"
#include "Renderer.hpp"
#include "SDLRenderer.hpp"
#include "imgui.h"

#include "../../external/ImGuiFileDialog/ImGuiFileDialog.h"
#include "core/ControlSettings.hpp"
#include "pcb/Board.hpp"
#include "pcb/BoardLoaderFactory.hpp"
#include "pcb/XZZPCBLoader.hpp"
#include "render/PcbRenderer.hpp"
#include "ui/MainMenuBar.hpp"
#include "ui/windows/PCBViewerWindow.hpp"
#include "ui/windows/PcbDetailsWindow.hpp"
#include "ui/windows/SettingsWindow.hpp"
#include "utils/StringUtils.hpp"
#include "view/Camera.hpp"
#include "view/Grid.hpp"
#include "view/GridSettings.hpp"
#include "view/Viewport.hpp"

namespace
{
std::string GetAppConfigFilePath()
{
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
                    std::cerr << "Error: Could not create preference directory: " << pathObj.string() << ". Falling back to current directory." << std::endl;
                    return std::filesystem::current_path().append(CONFIG_FILENAME).string();
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error creating directory " << pathObj.string() << ": " << e.what() << ". Falling back to current directory." << std::endl;
            return std::filesystem::current_path().append(CONFIG_FILENAME).string();
        }
    }
    pathObj /= CONFIG_FILENAME;
    return pathObj.string();
}
}  // namespace

Application::Application()
    : m_isRunning(false), m_appName("XZZPCB Layer Viewer"), m_windowWidth(800), m_windowHeight(600), m_clearColor {0.1f, 0.105f, 0.11f, 1.0f}, m_showPcbLoadErrorModal(false), m_pcbLoadErrorMessage("")
{
}

Application::~Application()
{
    Shutdown();
}

void Application::LoadConfig()
{
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
        std::string imguiIniDataUnescaped = string_utils::UnescapeNewlines(imguiIniDataEscaped);
        ImGui::LoadIniSettingsFromMemory(imguiIniDataUnescaped.c_str(), imguiIniDataUnescaped.length());
    }
    ImGui::GetIO().IniFilename = nullptr;

    m_events->SetImGuiManager(m_imguiManager.get());

    // Initialize PcbRenderer for Blend2D rendering

    return true;
}

bool Application::InitializeUISubsystems()
{
    m_camera = std::make_shared<Camera>();
    m_viewport = std::make_shared<Viewport>();
    m_gridSettings = std::make_shared<GridSettings>();
    m_grid = std::make_shared<Grid>(m_gridSettings);
    m_boardDataManager = std::make_shared<BoardDataManager>();

    // Load BoardDataManager settings from config
    if (m_config) {
        m_boardDataManager->LoadSettingsFromConfig(*m_config);
    }

    // Register for settings change callbacks to handle board folding changes
    if (m_boardDataManager) {
        m_boardDataManager->RegisterSettingsChangeCallback([this]() {
            std::cout << "Application: BoardDataManager settings changed, updating current board..." << std::endl;
            if (m_currentBoard && m_currentBoard->IsLoaded()) {
                std::cout << "Application: Applying folding state to current board" << std::endl;
                m_currentBoard->UpdateFoldingState();
            }
        });
    }

    // Initialize PCB Loader Factory
    m_boardLoaderFactory = std::make_unique<BoardLoaderFactory>();
    if (!m_boardLoaderFactory) {
        std::cerr << "Failed to create BoardLoaderFactory!" << std::endl;
        // return false; // Decide if this is a fatal error for application startup
    }

    m_mainMenuBar = std::make_unique<MainMenuBar>();
    m_pcbViewerWindow = std::make_unique<PCBViewerWindow>(m_camera, m_viewport, m_grid, m_gridSettings, m_controlSettings, m_boardDataManager);
    m_settingsWindow = std::make_unique<SettingsWindow>(m_gridSettings, m_controlSettings, m_boardDataManager, m_clearColor);
    m_pcbDetailsWindow = std::make_unique<PcbDetailsWindow>();

    // Initialize PcbRenderer with BoardDataManager
    m_pcbRenderer = std::make_unique<PcbRenderer>();
    if (!m_pcbRenderer->Initialize(m_windowWidth, m_windowHeight, m_boardDataManager)) {
        std::cerr << "Failed to initialize PcbRenderer!" << std::endl;
        return false;
    }

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

    if (m_boardDataManager && m_config) {
        m_boardDataManager->SaveSettingsToConfig(*m_config);
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

    if (m_pcbRenderer) {  // Shutdown PcbRenderer
        m_pcbRenderer->Shutdown();
        m_pcbRenderer.reset();
    }
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
    m_boardDataManager.reset();

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

void Application::RenderUI()
{
    // Handle main menu bar first, as it might affect the main viewport's WorkPos/WorkSize
    if (m_mainMenuBar) {
        m_mainMenuBar->RenderUI(*this);  // This calls ImGui::BeginMainMenuBar/EndMainMenuBar

        if (m_quitFileRequested) {
            Quit();
            m_quitFileRequested = false;
        }
    }

    // Create a DockSpace that covers the entire main viewport area below the main menu bar
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 mainWorkPos = viewport->WorkPos;    // Position of the work area (below main menu bar)
    ImVec2 mainWorkSize = viewport->WorkSize;  // Size of the work area

    // Set up window flags for the dockspace host window
    ImGuiWindowFlags dockspace_window_flags = ImGuiWindowFlags_NoDocking;  // This window itself cannot be docked
    dockspace_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    dockspace_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    dockspace_window_flags |= ImGuiWindowFlags_NoBackground;  // Make it transparent

    ImGui::SetNextWindowPos(mainWorkPos);
    ImGui::SetNextWindowSize(mainWorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));  // No padding for the dockspace window

    ImGui::Begin("RootDockspaceWindow", nullptr, dockspace_window_flags);
    ImGui::PopStyleVar(3);  // Pop WindowRounding, WindowBorderSize, WindowPadding

    ImGuiID dockspace_id = ImGui::GetID("ApplicationRootDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    // Consider ImGuiDockNodeFlags_PassthruCentralNode if you want to draw directly into the central node
    // when nothing is docked there, but for now, a standard dockspace is fine.

    ImGui::End();  // End of RootDockspaceWindow

    // Handle file dialog opening (m_openFileRequested is set by MainMenuBar)
    if (m_openFileRequested && m_fileDialogInstance && m_boardLoaderFactory) {
        std::string supportedExtensions = m_boardLoaderFactory->GetSupportedExtensionsFilterString();
        std::vector<std::string> extensionsList = m_boardLoaderFactory->GetSupportedExtensions();

        // Dynamically apply file styling for registered extensions
        // This is a basic example; you might want more sophisticated color/icon choices
        ImVec4 defaultColor1 = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Greenish
        ImVec4 defaultColor2 = ImVec4(0.2f, 0.5f, 0.8f, 1.0f);  // Bluish
        for (size_t i = 0; i < extensionsList.size(); ++i) {
            const auto& ext = extensionsList[i];
            // Simple alternating color for example
            ImVec4 color = (i % 2 == 0) ? defaultColor1 : defaultColor2;
            if (ext == ".kicad_pcb")
                color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Keep specific for kicad if wanted
            else if (ext == ".pcb")
                color = ImVec4(0.2f, 0.5f, 0.8f, 1.0f);  // Keep specific for .pcb if wanted

            m_fileDialogInstance->SetFileStyle(IGFD_FileStyleByExtention, ext.c_str(), color, "", nullptr);
        }

        IGFD::FileDialogConfig config;
        config.path = ".";  // Default path
        m_fileDialogInstance->OpenDialog("ChooseFileDlgKey", "Choose PCB File", supportedExtensions.empty() ? nullptr : supportedExtensions.c_str(), config);

        m_openFileRequested = false;  // Reset flag after initiating dialog opening
    }

    // Handle visibility requests for other windows, potentially set by MainMenuBar
    if (m_showSettingsRequested) {
        if (m_settingsWindow)
            m_settingsWindow->SetVisible(true);
        m_showSettingsRequested = false;  // Reset flag
    }

    if (m_showPcbDetailsRequested) {
        if (m_pcbDetailsWindow)
            m_pcbDetailsWindow->SetVisible(true);
        m_showPcbDetailsRequested = false;  // Reset flag
    }

    // Handle file dialog display and selection
    if (m_fileDialogInstance && m_fileDialogInstance->Display("ChooseFileDlgKey")) {
        if (m_fileDialogInstance->IsOk()) {
            std::string filePathName = m_fileDialogInstance->GetFilePathName();
            // std::string filePath = m_fileDialogInstance->GetCurrentPath();
            OpenPcbFile(filePathName);
        }
        m_fileDialogInstance->Close();
    }

    // --- PCBViewerWindow Rendering ---
    if (m_pcbViewerWindow && m_renderer && m_pcbRenderer) {
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
            // Call the integrated rendering method on PCBViewerWindow.
            // This method will handle ImGui::Begin/End, viewport updates,
            // then execute the provided lambda to render the PCB,
            // then update its texture and draw the ImGui::Image.
            m_pcbViewerWindow->RenderIntegrated(sdlRenderer,
                                                m_pcbRenderer.get(),
                                                [&]() {  // This is the pcbRenderCallback lambda
                                                    bool baseComponentsValid = m_camera && m_viewport && m_grid;
                                                    bool boardLoaded = m_currentBoard != nullptr;

                                                    if (baseComponentsValid) {
                                                        m_pcbRenderer->Render(boardLoaded ? m_currentBoard.get() : nullptr, m_camera.get(), m_viewport.get(), m_grid.get());
                                                    } else {
                                                        // This case should ideally be handled by PcbRenderer::Render itself
                                                        // by drawing a placeholder if components are missing.
                                                        // Logging here is for Application-level awareness if needed.
                                                        // SDL_Log("Application::RenderUI (lambda): Base component(s) for PcbRenderer::Render are NULL.");
                                                        // PcbRenderer::Render is designed to handle null components and draw a placeholder.
                                                        m_pcbRenderer->Render(nullptr, nullptr, nullptr, nullptr);
                                                    }
                                                });
        }
    }
    // --- End PCBViewerWindow Rendering ---

    if (m_settingsWindow) {
        m_settingsWindow->RenderUI(m_currentBoard);
    }

    if (m_pcbDetailsWindow) {
        if (m_pcbDetailsWindow->IsWindowVisible()) {
            m_pcbDetailsWindow->Render();
        }
    }

    if (m_showPcbLoadErrorModal) {
        ImGui::OpenPopup("PCB Load Error");
        m_showPcbLoadErrorModal = false;  // Reset flag
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
    // 1. Start ImGui Frame (calls ImGui_ImplSDLRenderer3_NewFrame, ImGui_ImplSDL3_NewFrame, ImGui::NewFrame)
    m_imguiManager->NewFrame();

    // 2. Define all ImGui windows and their content.
    //    This populates ImGui's internal draw lists.
    //    PCBViewerWindow::RenderIntegrated will call its pcbRenderCallback,
    //    which in turn calls PcbRenderer::Render().
    RenderUI();

    // 3. Finalize ImGui's draw data (calls ImGui::Render())
    m_imguiManager->FinalizeImGuiDrawLists();

    // 4. Clear SDL backbuffer with the application's clear color
    m_renderer->Clear();

    // 5. Render ImGui's finalized draw data to the SDL backbuffer
    //    (calls ImGui_ImplSDLRenderer3_RenderDrawData)
    m_imguiManager->PresentImGuiDrawData();

    // 6. Present the SDL backbuffer to the screen
    m_renderer->Present();
}

void Application::OpenPcbFile(const std::string& filePath)
{
    if (!m_boardLoaderFactory) {
        std::cerr << "Error: BoardLoaderFactory not initialized in Application::OpenPcbFile." << std::endl;
        return;
    }

    auto newBoard = m_boardLoaderFactory->LoadBoard(filePath);
    if (newBoard) {
        m_currentBoard = std::move(newBoard);

        // Set up the BoardDataManager reference in the board
        if (m_boardDataManager) {
            m_currentBoard->SetBoardDataManager(m_boardDataManager);
            m_boardDataManager->SetBoard(m_currentBoard);
            m_boardDataManager->RegenerateLayerColors(m_currentBoard);

            // Apply folding if enabled
            std::cout << "Application: Checking if board folding should be applied..." << std::endl;
            if (m_boardDataManager->IsBoardFoldingEnabled()) {
                std::cout << "Application: Board folding is enabled, applying to new board" << std::endl;
                m_currentBoard->UpdateFoldingState();
            }
        }

        // Corrected window updates:
        if (m_pcbViewerWindow) {
            // Assuming PCBViewerWindow has a method to accept the new board
            // This might be OnBoardLoaded(m_currentBoard) or SetBoard(m_currentBoard) or similar
            // For now, let's assume it might need to know about the board directly.
            // If PCBViewerWindow gets the board via BoardDataManager, this direct call might not be needed.
            // m_pcbViewerWindow->SetBoard(m_currentBoard); // Or similar method
        }
        if (m_pcbDetailsWindow) {
            m_pcbDetailsWindow->SetBoard(m_currentBoard);  // Update details window
        }

        if (m_camera && m_viewport && m_currentBoard) {
            BLRect board_bounds = m_currentBoard->GetBoundingBox(true);
            std::cout << "Board Bounding Box for FocusOnRect: X=" << board_bounds.x << " Y=" << board_bounds.y << " W=" << board_bounds.w << " H=" << board_bounds.h << std::endl;

            m_camera->FocusOnRect(board_bounds, *m_viewport, 0.1f);

            std::cout << "Camera after FocusOnRect: Zoom=" << m_camera->GetZoom() << " Position=(" << m_camera->GetPosition().x_ax << "," << m_camera->GetPosition().y_ax << ")"
                      << " Rotation=" << m_camera->GetRotation() << std::endl;
            if (m_viewport) {
                std::cout << "Viewport: Width=" << m_viewport->GetWidth() << " Height=" << m_viewport->GetHeight() << std::endl;
            }
        }
        std::cout << "Successfully loaded PCB: " << filePath << std::endl;
        if (m_currentBoard) {
            std::cout << "Board dimensions: " << m_currentBoard->width << " x " << m_currentBoard->height << std::endl;
            std::cout << "Board origin offset: " << m_currentBoard->origin_offset.x << ", " << m_currentBoard->origin_offset.y << std::endl;
        }

        // Apply layer properties (colors, etc.) using BoardDataManager
        if (m_boardDataManager) {
            m_boardDataManager->RegenerateLayerColors(m_currentBoard);
            m_boardDataManager->SetBoard(m_currentBoard);
        } else {
            std::cerr << "Application::OpenPcbFile: BoardDataManager is null, cannot apply layer properties or set board." << std::endl;
            // Decide if a board should be considered valid if BoardDataManager is missing
            // For now, proceed but log error. Or, could `return;` here.
        }
    } else {
        std::cerr << "Failed to load PCB: " << filePath << std::endl;
        if (m_camera && m_viewport) {
            m_camera->Reset();
        }
        m_currentBoard = nullptr;  // Ensure current board is null if load failed
        if (m_boardDataManager) {
            m_boardDataManager->SetBoard(nullptr);
        }
        // TODO: Inform UI or user about the failure
    }
}

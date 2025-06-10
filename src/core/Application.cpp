#include "Application.hpp"

#include <filesystem>
#include <iostream>
#include <thread>

#include "Events.hpp" // For WindowEventType

#include <SDL3/SDL_filesystem.h>

#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(__WIN64__) || defined(WIN64) || defined(_WIN64) || defined(_MSC_VER)
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#endif

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
    m_controlSettings->LoadSettingsFromConfig(*m_config);
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

    // Set up window event handling for minimization/restoration
    m_events->SetWindowEventCallback([this](WindowEventType event_type) {
        HandleWindowEvent(event_type);
    });

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

    // Load settings from config
    if (m_config) {
        m_gridSettings->LoadSettingsFromConfig(*m_config);
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

    // Initialize PcbRenderer with BoardDataManager first
    m_pcbRenderer = std::make_unique<PcbRenderer>();
    if (!m_pcbRenderer->Initialize(m_windowWidth, m_windowHeight, m_boardDataManager)) {
        std::cerr << "Failed to initialize PcbRenderer!" << std::endl;
        return false;
    }

    // Create SettingsWindow with Grid for font invalidation
    m_settingsWindow = CreateSettingsWindow(m_gridSettings, m_controlSettings, m_boardDataManager, m_clearColor, m_grid);
    m_pcbDetailsWindow = std::make_unique<PcbDetailsWindow>();

    // Load font settings after SettingsWindow is created
    if (m_config && m_settingsWindow) {
        m_settingsWindow->LoadFontSettingsFromConfig(*m_config);
    }

    m_fileDialogInstance = std::make_unique<ImGuiFileDialog>();

    // Initialize file dialog with side-panel features
    InitializeFileDialogPlaces();

    m_settingsWindow->SetVisible(true);
    m_pcbDetailsWindow->SetVisible(false);

    return true;
}

void Application::InitializeFileDialogPlaces()
{
#ifdef USE_PLACES_FEATURE
    // Add common places group for the file dialog
    const char* commonPlacesGroup = "Common Places";
    m_fileDialogInstance->AddPlacesGroup(commonPlacesGroup, 0, false, true);

    auto commonPlaces = m_fileDialogInstance->GetPlacesGroupPtr(commonPlacesGroup);
    if (commonPlaces != nullptr) {
        // Add common system locations //Had started to implement a system to use nerd font icons, but some of the extended glyph sets are not working well with ImGui, so it's disabled for now
#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(__WIN64__) || defined(WIN64) || defined(_WIN64) || defined(_MSC_VER)
        // Windows-specific places
        PWSTR path = NULL;
        HRESULT hr;

        // Desktop
        hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &path);
        if (SUCCEEDED(hr)) {
            IGFD::FileStyle style;
            // style.icon = "";  // Desktop icon
            auto place_path = IGFD::Utils::UTF8Encode(path);
            commonPlaces->AddPlace("Desktop", place_path, false, style);
            CoTaskMemFree(path);
        }

        // Documents
        hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &path);
        if (SUCCEEDED(hr)) {
            IGFD::FileStyle style;
            style.icon = "";  // Folder icon
            auto place_path = IGFD::Utils::UTF8Encode(path);
            commonPlaces->AddPlace("Documents", place_path, false, style);
            CoTaskMemFree(path);
        }

        // Downloads
        hr = SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &path);
        if (SUCCEEDED(hr)) {
            IGFD::FileStyle style;
            style.icon = "";  // Download icon
            auto place_path = IGFD::Utils::UTF8Encode(path);
            commonPlaces->AddPlace("Downloads", place_path, false, style);
            CoTaskMemFree(path);
        }
#else
        // Unix/Linux/macOS places
        const char* homeDir = getenv("HOME");
        if (homeDir) {
            IGFD::FileStyle style;

            // Home directory
            // style.icon = "";  // Home icon
            commonPlaces->AddPlace("Home", homeDir, false, style);

            // Desktop (if exists)
            std::string desktopPath = std::string(homeDir) + "/Desktop";
            if (std::filesystem::exists(desktopPath)) {
                // style.icon = "";  // Desktop icon
                commonPlaces->AddPlace("Desktop", desktopPath, false, style);
            }

            // Documents (if exists)
            std::string documentsPath = std::string(homeDir) + "/Documents";
            if (std::filesystem::exists(documentsPath)) {
                // style.icon = "";  // Folder icon
                commonPlaces->AddPlace("Documents", documentsPath, false, style);
            }

            // Downloads (if exists)
            std::string downloadsPath = std::string(homeDir) + "/Downloads";
            if (std::filesystem::exists(downloadsPath)) {
                // style.icon = "";  // Download icon
                commonPlaces->AddPlace("Downloads", downloadsPath, false, style);
            }
        }
#endif

        // Add current directory
        IGFD::FileStyle style;
        // style.icon = "";  // Current folder icon
        commonPlaces->AddPlace("Current Directory", ".", false, style);
    }

    // Add user bookmarks group (editable)
    const char* bookmarksGroup = "Bookmarks";
    m_fileDialogInstance->AddPlacesGroup(bookmarksGroup, 1, true, true);

    // Load existing bookmarks from config (must be done after creating the bookmarks group)
    LoadFileDialogBookmarks();

#ifdef USE_PLACES_DEVICES
    // Devices will be automatically added by ImGuiFileDialog if USE_PLACES_DEVICES is defined
#endif

#endif // USE_PLACES_FEATURE
}

void Application::LoadFileDialogBookmarks()
{
#ifdef USE_PLACES_FEATURE
    if (m_config && m_fileDialogInstance) {
        std::string bookmarksData = m_config->GetString("file_dialog.bookmarks", "");

        // Debug output to understand what's being loaded
        std::cout << "LoadFileDialogBookmarks: Raw loaded data: '" << bookmarksData << "'" << std::endl;

        if (!bookmarksData.empty()) {
            m_fileDialogInstance->DeserializePlaces(bookmarksData);

            // Check what was actually loaded
            auto bookmarksGroup = m_fileDialogInstance->GetPlacesGroupPtr("Bookmarks");
            if (bookmarksGroup) {
                std::cout << "LoadFileDialogBookmarks: After loading, Bookmarks group has " << bookmarksGroup->places.size() << " places" << std::endl;
                for (size_t i = 0; i < bookmarksGroup->places.size(); ++i) {
                    const auto& place = bookmarksGroup->places[i];
                    std::cout << "  Loaded Place " << i << ": name='" << place.name << "', path='" << place.path << "'" << std::endl;
                }
            } else {
                std::cout << "LoadFileDialogBookmarks: No Bookmarks group found after loading!" << std::endl;
            }
        } else {
            std::cout << "LoadFileDialogBookmarks: No bookmark data to load" << std::endl;
        }
    }
#endif // USE_PLACES_FEATURE
}

void Application::SaveFileDialogBookmarks()
{
#ifdef USE_PLACES_FEATURE
    if (m_config && m_fileDialogInstance) {
        // Save bookmarks from the main file dialog instance (both should have the same bookmarks)
        std::string bookmarksData = m_fileDialogInstance->SerializePlaces(false); // Don't serialize code-based places

        // Debug output to understand what's being serialized
        std::cout << "SaveFileDialogBookmarks: Raw serialized data: '" << bookmarksData << "'" << std::endl;

        // Check if we have any bookmarks groups and their contents
        auto bookmarksGroup = m_fileDialogInstance->GetPlacesGroupPtr("Bookmarks");
        if (bookmarksGroup) {
            std::cout << "SaveFileDialogBookmarks: Bookmarks group found with " << bookmarksGroup->places.size() << " places" << std::endl;
            std::cout << "SaveFileDialogBookmarks: Group canBeSaved=" << (bookmarksGroup->canBeSaved ? "true" : "false") << std::endl;
            for (size_t i = 0; i < bookmarksGroup->places.size(); ++i) {
                const auto& place = bookmarksGroup->places[i];
                std::cout << "  Place " << i << ": name='" << place.name << "', path='" << place.path << "', canBeSaved=" << (place.canBeSaved ? "true" : "false") << std::endl;
            }
        } else {
            std::cout << "SaveFileDialogBookmarks: No Bookmarks group found!" << std::endl;
        }

        m_config->SetString("file_dialog.bookmarks", bookmarksData);
    }
#endif // USE_PLACES_FEATURE
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

        // Note: Framerate limiting removed - VSync in SDL renderer handles frame timing
        // This eliminates the performance bottleneck from std::this_thread::sleep_for()
        // Target framerate setting is now used only for display/configuration purposes
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
        m_controlSettings->SaveSettingsToConfig(*m_config);
    }

    if (m_gridSettings && m_config) {
        m_gridSettings->SaveSettingsToConfig(*m_config);
    }

    if (m_boardDataManager && m_config) {
        m_boardDataManager->SaveSettingsToConfig(*m_config);
    }

    if (m_settingsWindow && m_config) {
        m_settingsWindow->SaveFontSettingsToConfig(*m_config);
    }

    // Save file dialog bookmarks
    SaveFileDialogBookmarks();

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

void Application::ProcessGlobalKeyboardShortcuts()
{
    // Debug output to understand blocking conditions
    static bool debug_logged = false;
    if (!debug_logged) {
        std::cout << "ProcessGlobalKeyboardShortcuts: m_controlSettings=" << (m_controlSettings ? "valid" : "null")
                  << ", WantCaptureKeyboard=" << ImGui::GetIO().WantCaptureKeyboard << std::endl;
        debug_logged = true;
    }

    // Only process shortcuts if we have control settings and ImGui is not capturing keyboard
    if (!m_controlSettings) {
        return;
    }

    // For global shortcuts like Ctrl+O, we should process them even if ImGui wants keyboard
    // Only skip if we're actively typing in a text field
    if (ImGui::GetIO().WantTextInput) {
        return;
    }

    // Helper function to check if a keybind is active (similar to NavigationTool)
    auto IsKeybindActive = [](const KeyCombination& kb, ImGuiIO& io) -> bool {
        if (!kb.IsBound()) {
            return false;
        }

        // Check if the key was just pressed (not held down)
        if (!ImGui::IsKeyPressed(kb.key, false)) {
            return false;
        }

        // Check modifiers
        if (kb.ctrl && !io.KeyCtrl) {
            return false;
        }
        if (kb.shift && !io.KeyShift) {
            return false;
        }
        if (kb.alt && !io.KeyAlt) {
            return false;
        }

        // Ensure unwanted modifiers are not pressed
        if (!kb.ctrl && io.KeyCtrl) {
            return false;
        }
        if (!kb.shift && io.KeyShift) {
            return false;
        }
        if (!kb.alt && io.KeyAlt) {
            return false;
        }

        return true;
    };

    ImGuiIO& io = ImGui::GetIO();

    // Check for Ctrl+O (Open File)
    KeyCombination openFileKeybind = m_controlSettings->GetKeybind(InputAction::kOpenFile);

    // Debug output for Ctrl+O specifically
    if (ImGui::IsKeyPressed(ImGuiKey_O, false) && io.KeyCtrl) {
        std::cout << "Ctrl+O detected! Keybind config - Key: " << static_cast<int>(openFileKeybind.key)
                  << " (O=" << static_cast<int>(ImGuiKey_O) << ")"
                  << ", Ctrl: " << openFileKeybind.ctrl
                  << ", Shift: " << openFileKeybind.shift
                  << ", Alt: " << openFileKeybind.alt << std::endl;
    }

    if (IsKeybindActive(openFileKeybind, io)) {
        m_showFileDialogWindow = true;  // Set the flag to trigger dockable file dialog opening
        std::cout << "Global shortcut: Open file dialog triggered" << std::endl;
    }
}

void Application::RenderUI()
{
    // Process global keyboard shortcuts first
    ProcessGlobalKeyboardShortcuts();

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

    // --- File Dialog Window ---
    if (m_showFileDialogWindow && m_fileDialogInstance) {
        RenderFileDialog();
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

        // Set up the BoardDataManager and ControlSettings references in the board
        if (m_boardDataManager) {
            m_currentBoard->SetBoardDataManager(m_boardDataManager);
            m_boardDataManager->SetBoard(m_currentBoard);
            m_boardDataManager->RegenerateLayerColors(m_currentBoard);
        }
        if (m_controlSettings) {
            m_currentBoard->SetControlSettings(m_controlSettings);

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

void Application::RenderFileDialog()
{
    if (!m_fileDialogInstance || !m_boardLoaderFactory) {
        return;
    }

    // Create a dockable window for the file dialog
    ImGui::Begin("File Browser", &m_showFileDialogWindow, ImGuiWindowFlags_None);

    // Get the current window size for clamping
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 minSize = ImVec2(400.0f, 300.0f);
    ImVec2 maxSize = ImGui::GetMainViewport()->WorkSize;
    maxSize.x *= 0.8f; // Limit to 80% of screen width
    maxSize.y *= 0.8f; // Limit to 80% of screen height

    // Clamp the window size
    if (windowSize.x < minSize.x || windowSize.y < minSize.y ||
        windowSize.x > maxSize.x || windowSize.y > maxSize.y) {
        windowSize.x = std::max(minSize.x, std::min(windowSize.x, maxSize.x));
        windowSize.y = std::max(minSize.y, std::min(windowSize.y, maxSize.y));
        ImGui::SetWindowSize(windowSize);
    }

    // Set up the embedded file dialog if not already open
    static bool dialog_initialized = false;
    if (!dialog_initialized) {
        std::string supported_extensions = m_boardLoaderFactory->GetSupportedExtensionsFilterString();
        std::vector<std::string> extensions_list = m_boardLoaderFactory->GetSupportedExtensions();

        // Apply file styling for registered extensions (migrated from modal dialog)
        ImVec4 default_color_1 = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Greenish
        ImVec4 default_color_2 = ImVec4(0.2f, 0.5f, 0.8f, 1.0f);  // Bluish
        for (size_t i = 0; i < extensions_list.size(); ++i) {
            const auto& ext = extensions_list[i];
            // Simple alternating color for example
            ImVec4 color = (i % 2 == 0) ? default_color_1 : default_color_2;
            if (ext == ".kicad_pcb")
                color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Keep specific for kicad if wanted
            else if (ext == ".pcb")
                color = ImVec4(0.2f, 0.5f, 0.8f, 1.0f);  // Keep specific for .pcb if wanted

            m_fileDialogInstance->SetFileStyle(IGFD_FileStyleByExtention, ext.c_str(), color, "", nullptr);
        }

        IGFD::FileDialogConfig config;
        config.path = ".";
        config.countSelectionMax = 1;
        config.flags = ImGuiFileDialogFlags_NoDialog | ImGuiFileDialogFlags_ShowDevicesButton;

        m_fileDialogInstance->OpenDialog("EmbeddedFileDialog", "Select PCB File",
                                        supported_extensions.empty() ? nullptr : supported_extensions.c_str(),
                                        config);
        dialog_initialized = true;
    }

    // Display the embedded file dialog
    ImVec2 content_size = ImGui::GetContentRegionAvail();
    if (m_fileDialogInstance->Display("EmbeddedFileDialog", ImGuiWindowFlags_None, content_size)) {
        if (m_fileDialogInstance->IsOk()) {
            // User selected a file
            std::string file_path_name = m_fileDialogInstance->GetFilePathName();
            OpenPcbFile(file_path_name);
            m_showFileDialogWindow = false; // Close the window after file selection
            dialog_initialized = false; // Reset for next time
        } else {
            // User cancelled the dialog
            m_showFileDialogWindow = false; // Close the window after cancellation
            dialog_initialized = false; // Reset for next time
        }
        // Note: Don't close the dialog here as it's embedded
    }

    ImGui::End();

    // Reset dialog initialization if window was closed
    if (!m_showFileDialogWindow) {
        dialog_initialized = false;
        m_fileDialogInstance->Close();
    }
}

void Application::ClampFileDialogSize()
{
    // This method can be used to clamp modal file dialog sizes
    // For now, we handle clamping in the modal dialog display call
    // by using ImGui's size constraints in the Display() call
}

void Application::HandleWindowEvent(WindowEventType event_type)
{
    switch (event_type) {
        case WindowEventType::kMinimized:
            std::cout << "Application: Window minimized - renderer context may become invalid" << std::endl;
            // Mark renderer as needing recreation when window is restored
            if (m_pcbRenderer) {
                // Force a full redraw when window is restored
                m_pcbRenderer->MarkFullRedrawNeeded();
            }
            break;

        case WindowEventType::kRestored:
            std::cout << "Application: Window restored - recreating renderer context if needed" << std::endl;
            // Recreate renderer context if it became invalid during minimization
            if (m_renderer && m_pcbRenderer) {
                bool renderer_recreated = false;

                // Check if SDL renderer is still valid
                auto sdlRenderer = dynamic_cast<SDLRenderer*>(m_renderer.get());
                if (sdlRenderer && !sdlRenderer->IsValid()) {
                    std::cout << "Application: SDL renderer invalid, attempting to recreate..." << std::endl;
                    // Attempt to recreate the SDL renderer
                    if (sdlRenderer->Recreate()) {
                        renderer_recreated = true;
                        std::cout << "Application: Successfully recreated SDL renderer" << std::endl;
                    } else {
                        std::cerr << "Application: Failed to recreate SDL renderer after window restoration!" << std::endl;
                    }
                }

                // If renderer was recreated, also recreate ImGui backend
                if (renderer_recreated && m_imguiManager) {
                    std::cout << "Application: Recreating ImGui backend after renderer recreation..." << std::endl;
                    m_imguiManager->OnRendererRecreated();
                }

                // Force a full redraw to refresh the display
                m_pcbRenderer->MarkFullRedrawNeeded();
                m_pcbRenderer->MarkBoardDirty();
                m_pcbRenderer->MarkGridDirty();
            }
            break;

        case WindowEventType::kShown:
            std::cout << "Application: Window shown" << std::endl;
            if (m_pcbRenderer) {
                m_pcbRenderer->MarkFullRedrawNeeded();
            }
            break;

        case WindowEventType::kHidden:
            std::cout << "Application: Window hidden" << std::endl;
            break;
    }
}

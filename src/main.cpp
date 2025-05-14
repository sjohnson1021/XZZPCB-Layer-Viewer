#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <stdio.h>
#include <memory>

// Project includes
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include "view/GridSettings.hpp"
#include "view/Grid.hpp"
#include "ui/windows/PCBViewerWindow.hpp"
#include "ui/windows/SettingsWindow.hpp"
#include "ui/windows/PcbDetailsWindow.hpp"
#include "pcb/PcbLoader.hpp"
#include "pcb/Board.hpp"
#include "ImGuiFileDialog.h"
#include <SDL3/SDL_events.h>

// Screen dimension constants (or load from config)
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

// Global or application-level state
std::shared_ptr<Board> currentBoard = nullptr;
PcbDetailsWindow pcbDetailsWindow;

void SetupImGuiStyle() {
    ImGui::StyleColorsDark(); // or ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    // Enable docking
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

void RenderMainMenuBar(SettingsWindow& settingsWindow) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open...")) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose PCB File", ".pcb", config);
            }
            if (ImGui::MenuItem("Exit")) {
                SDL_Event quit_event;
                quit_event.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&quit_event);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Settings", nullptr, settingsWindow.IsWindowVisible())) {
                settingsWindow.SetVisible(!settingsWindow.IsWindowVisible());
            }
            // Add other view options here (e.g., toggle PCBViewerWindow if it could be closed)
            ImGui::EndMenu();
        }
        // Add other menus (Edit, Tools, Help, etc.)
        ImGui::EndMainMenuBar();
    }

    // Handle File Dialog
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();

            PcbLoader loader;
            currentBoard = loader.loadFromFile(filePathName);
            if (currentBoard) {
                pcbDetailsWindow.setBoard(currentBoard);
                pcbDetailsWindow.SetVisible(true);
            } else {
                SDL_Log("Failed to load PCB file: %s", filePathName.c_str());
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

int main(int argc, char* args[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("XZZPCB Layer Viewer",
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL );
    if (window == nullptr) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    if (SDL_SetRenderVSync(renderer, 1) < 0) {
        printf("Warning: VSync not enabled! SDL_Error: %s\n", SDL_GetError());
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    SetupImGuiStyle();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    auto camera = std::make_shared<Camera>();
    auto viewport = std::make_shared<Viewport>();
    auto gridSettings = std::make_shared<GridSettings>();
    auto grid = std::make_shared<Grid>(gridSettings);

    PCBViewerWindow pcbViewerWindow(camera, viewport, grid, gridSettings);
    SettingsWindow settingsWindow(gridSettings);
    settingsWindow.SetVisible(true);

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            }
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGuiWindowFlags dockspaceFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        dockspaceFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        dockspaceFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGui::Begin("MainDockSpace", nullptr, dockspaceFlags);
        ImGui::PopStyleVar(3);
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        RenderMainMenuBar(settingsWindow);
        pcbViewerWindow.RenderUI(renderer);
        settingsWindow.RenderUI();
        if (currentBoard) {
            pcbDetailsWindow.render();
        }
        ImGui::End();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
} 
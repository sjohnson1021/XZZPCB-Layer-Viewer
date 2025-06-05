#include "ImGuiManager.hpp"

#include <imgui.h>
#include <iostream>

#include <../external/imgui/backends/imgui_impl_sdl3.h>
#include <../external/imgui/backends/imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL_events.h>

#include "Renderer.hpp"
#include "SDLRenderer.hpp"

ImGuiManager::ImGuiManager(Renderer* renderer) : m_renderer_(renderer), m_initialized_(false) {}

ImGuiManager::~ImGuiManager()
{
    Shutdown();
}

bool ImGuiManager::Initialize()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking

    // Performance optimization settings
    io.ConfigMemoryCompactTimer = 8.0F;           // Compact memory less frequently
    io.ConfigWindowsMoveFromTitleBarOnly = true;  // Only move windows from title bar

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    SDLRenderer* sdl_renderer = dynamic_cast<SDLRenderer*>(m_renderer_);
    if (sdl_renderer != nullptr) {
        ImGui_ImplSDL3_InitForSDLRenderer(sdl_renderer->GetWindow(), sdl_renderer->GetRenderer());
        ImGui_ImplSDLRenderer3_Init(sdl_renderer->GetRenderer());
    } else {
        std::cerr << "ImGuiManager requires an SDLRenderer" << std::endl;
        return false;
    }

    m_initialized_ = true;
    return true;
}

void ImGuiManager::Shutdown()
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiManager::ProcessEvent(SDL_Event* event)
{
    ImGui_ImplSDL3_ProcessEvent(event);
}

void ImGuiManager::NewFrame()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::FinalizeImGuiDrawLists()
{
    if (!m_initialized_) {
        return;
    }
    ImGui::Render();
}

void ImGuiManager::PresentImGuiDrawData()
{
    if (!m_initialized_ || !m_renderer_) {
        return;
    }
    SDLRenderer* sdl_renderer = dynamic_cast<SDLRenderer*>(m_renderer_);
    if ((sdl_renderer != nullptr) && (sdl_renderer->GetRenderer() != nullptr)) {
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdl_renderer->GetRenderer());
    } else {
        std::cerr << "ImGuiManager::PresentImGuiDrawData: SDLRenderer or SDL_Renderer handle is null." << std::endl;
    }
}

#include "ImGuiManager.hpp"
#include "Renderer.hpp"
#include "SDLRenderer.hpp"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include "../utils/OpenGLUtils.hpp"
#include <iostream>

ImGuiManager::ImGuiManager(Renderer* renderer)
    : m_renderer(renderer)
    , m_initialized(false)
{
}

ImGuiManager::~ImGuiManager()
{
    Shutdown();
}

bool ImGuiManager::Initialize()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    
    // Performance optimization settings
    io.ConfigMemoryCompactTimer = 8.0f;  // Compact memory less frequently
    io.ConfigWindowsMoveFromTitleBarOnly = true;  // Only move windows from title bar

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    SDLRenderer* sdlRenderer = dynamic_cast<SDLRenderer*>(m_renderer);
    if (sdlRenderer) {
        ImGui_ImplSDL3_InitForSDLRenderer(sdlRenderer->GetWindow(), sdlRenderer->GetRenderer());
        ImGui_ImplSDLRenderer3_Init(sdlRenderer->GetRenderer());
    } else {
        std::cerr << "ImGuiManager requires an SDLRenderer" << std::endl;
        return false;
    }

    m_initialized = true;
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

void ImGuiManager::Render()
{
    if (!m_initialized || !m_renderer) {
        return;
    }

    // Check for GL errors before ImGui::Render()
    GL_CHECK_ERRORS();

    ImGui::Render();

    // Check for GL errors after ImGui::Render() but before backend RenderDrawData
    GL_CHECK_ERRORS();

    // The ImGui_ImplSDLRenderer3_RenderDrawData function will make many OpenGL calls.
    // We pass the SDL_Renderer*, and the impl backend knows how to use it for GL operations.
    if (auto sdlRenderer = dynamic_cast<SDLRenderer*>(m_renderer)) {
        // TEMPORARILY COMMENTED OUT FOR TESTING THE NewFrame ASSERTION
        GL_CHECK(ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdlRenderer->GetRenderer()));
    } else {
        // Handle case where renderer is not SDLRenderer or provide a generic way if possible
        // For now, assuming it's SDLRenderer as per typical setup with this backend.
        // If you have a direct void* GetRendererHandle() that returns the SDL_Renderer*:
        // GL_CHECK(ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), static_cast<SDL_Renderer*>(m_renderer->GetRendererHandle())));
        std::cerr << "ImGuiManager::Render: Renderer is not of type SDLRenderer, cannot call ImGui_ImplSDLRenderer3_RenderDrawData properly." << std::endl;
    }

    // Check for GL errors after backend RenderDrawData would have been called
    GL_CHECK_ERRORS();
} 
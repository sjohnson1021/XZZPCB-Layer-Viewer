#include "ImGuiRenderer.hpp"
#include "Renderer.hpp"

ImGuiRenderer::ImGuiRenderer(Renderer* renderer)
    : m_renderer(renderer)
    , m_initialized(false)
{
}

ImGuiRenderer::~ImGuiRenderer() {
    Shutdown();
}

bool ImGuiRenderer::Initialize() {
    if (!m_renderer) {
        return false;
    }
    
    m_initialized = true;
    return true;
}

void ImGuiRenderer::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    m_initialized = false;
} 
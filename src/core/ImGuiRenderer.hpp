#pragma once

class Renderer;

class ImGuiRenderer {
public:
    ImGuiRenderer(Renderer* renderer);
    ~ImGuiRenderer();

    bool Initialize();
    void Shutdown();
    
    // Custom ImGui rendering methods can be added here
    
private:
    Renderer* m_renderer;
    bool m_initialized;
}; 
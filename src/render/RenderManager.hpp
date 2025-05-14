#pragma once

#include <memory>

// Forward declarations (if any become necessary)
// class RenderContext;
// class RenderPipeline;

class RenderManager {
public:
    RenderManager();
    ~RenderManager();

    RenderManager(const RenderManager&) = delete;
    RenderManager& operator=(const RenderManager&) = delete;
    RenderManager(RenderManager&&) = delete;
    RenderManager& operator=(RenderManager&&) = delete;

    void Initialize();
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void Render();

    // Other potential methods:
    // void SetRenderTarget(RenderTarget* target);
    // void RegisterRenderable(IRenderable* renderable);
    // void UnregisterRenderable(IRenderable* renderable);
    // RenderContext& GetContext();
    // RenderPipeline& GetPipeline();

private:
    // std::unique_ptr<RenderContext> m_context;
    // std::unique_ptr<RenderPipeline> m_pipeline;
    // std::vector<IRenderable*> m_renderables;
}; 
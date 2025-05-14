#pragma once

#include <vector>
#include <memory>

// Forward declarations
class RenderContext;
// class RenderStage; // If we define a base class for render stages
// class IRenderable; // Interface for objects that can be rendered

class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;
    RenderPipeline(RenderPipeline&&) = delete;
    RenderPipeline& operator=(RenderPipeline&&) = delete;

    bool Initialize(RenderContext& context);
    void Shutdown();

    // Called by RenderManager before processing renderables
    void BeginPipeline(RenderContext& context);
    // Called by RenderManager after all renderables are processed
    void EndPipeline(RenderContext& context);

    // Process a single renderable item (or this could be a list)
    // void Process(RenderContext& context, IRenderable* renderable);
    // Or, the pipeline might manage a list of renderables itself
    // void AddRenderable(IRenderable* renderable);
    // void RemoveRenderable(IRenderable* renderable);
    // void Execute(RenderContext& context); // Executes all stages on all renderables

    // If we have distinct stages:
    // void AddStage(std::unique_ptr<RenderStage> stage);

private:
    // std::vector<std::unique_ptr<RenderStage>> m_stages;
    // Or a more direct approach if stages are fixed:
    // void GeometryPass(RenderContext& context);
    // void LightingPass(RenderContext& context);
    // void PostProcessingPass(RenderContext& context);
    // void UIPass(RenderContext& context);

    bool m_initialized = false;
}; 
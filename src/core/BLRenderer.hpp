#pragma once

#include "Renderer.hpp"

class BLRenderer : public Renderer {
public:
    BLRenderer();
    ~BLRenderer() override;

    bool Initialize(const std::string& windowTitle, int windowWidth, int windowHeight) override;
    void Shutdown() override;

    void Clear() override;
    void Present() override;

    void* GetWindowHandle() const override;
    void* GetRendererHandle() const override;

    int GetWindowWidth() const override;
    int GetWindowHeight() const override;

private:
    // To be implemented
}; 
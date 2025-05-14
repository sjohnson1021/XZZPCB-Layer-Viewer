#pragma once

#include <string>

class Renderer {
public:
    Renderer();
    virtual ~Renderer();

    virtual bool Initialize(const std::string& title, int width, int height) = 0;
    virtual void Shutdown() = 0;
    virtual void Clear() = 0;
    virtual void Present() = 0;

    // Window and renderer handles/properties
    virtual void* GetWindowHandle() const = 0;
    virtual void* GetRendererHandle() const = 0;
    virtual int GetWindowWidth() const = 0;
    virtual int GetWindowHeight() const = 0;

    static Renderer* Create();
}; 
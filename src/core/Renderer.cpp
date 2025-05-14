#include "Renderer.hpp"
#include "SDLRenderer.hpp"

Renderer::Renderer() = default;
Renderer::~Renderer() = default;

Renderer* Renderer::Create()
{
    return new SDLRenderer();
} 
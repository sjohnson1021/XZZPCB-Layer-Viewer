#include "core/Renderer.hpp"
#include "core/SDLRenderer.hpp" // For SDLRenderer instantiation

// Default constructor
Renderer::Renderer() = default;

// Default virtual destructor
Renderer::~Renderer() = default;

// Static factory method to create a concrete Renderer instance
Renderer* Renderer::Create() {
    // Currently, we only have SDLRenderer as a concrete implementation
    return new SDLRenderer();
}
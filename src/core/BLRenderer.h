#pragma once

#include <SDL2/SDL.h>
#include <blend2d.h>
#include <memory>
#include <vector>
#include <string>

// Structure to hold Blend2D context and related objects
class BLRenderer {
private:
    // The actual image buffer we'll render to
    BLImage m_image;
    
    // Rendering context
    BLContext m_context;
    
    // SDL texture that will be updated with our rendered content
    SDL_Texture* m_texture;
    
    // Renderer reference
    SDL_Renderer* m_sdlRenderer;
    
    // Current dimensions
    int m_width;
    int m_height;
    
    // Flag to check if renderer is initialized
    bool m_initialized;
    
public:
    BLRenderer();
    ~BLRenderer();
    
    // Initialize or resize the renderer
    bool initialize(SDL_Renderer* renderer, int width, int height);
    
    // Resize if dimensions change
    bool resize(int width, int height);
    
    // Begin drawing operations
    void beginFrame();
    
    // End drawing and present to SDL
    void endFrame();
    
    // Get the Blend2D context for direct drawing
    BLContext& getContext() { return m_context; }
    
    // Drawing primitives that match the existing SDL functions
    void drawLine(float x1, float y1, float x2, float y2, const BLRgba32& color, float thickness = 1.0f);
    void drawCircle(float x, float y, float radius, const BLRgba32& color, bool filled = false);
    void drawRect(float x, float y, float width, float height, const BLRgba32& color, bool filled = false);
    void drawRoundRect(float x, float y, float width, float height, float radius, const BLRgba32& color, bool filled = false);
    void drawArc(float x, float y, float radius, float startAngle, float endAngle, const BLRgba32& color, float thickness = 1.0f);
    
    // Draw text without using fonts (using vector shapes)
    void drawText(float x, float y, const std::string& text, const BLRgba32& color);
    
    // Convert SDL_Color to BLRgba32
    static BLRgba32 toBlendColor(const SDL_Color& color) {
        return BLRgba32(color.r, color.g, color.b, color.a);
    }
}; 
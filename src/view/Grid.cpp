#include "view/Grid.hpp"
#include "view/GridSettings.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include <SDL3/SDL.h>
#include <cmath> // For std::floor, std::ceil, std::abs, std::fmod, std::min, std::max
#include <algorithm> // For std::min/max with initializer list
#include <vector>

// Define PI if not already available from Camera.cpp or a common math header
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Grid::Grid(std::shared_ptr<GridSettings> settings)
    : m_settings(settings) {}

Grid::~Grid() {}

void Grid::ConvertAndSetSDLColor(SDL_Renderer* renderer, const GridColor& gridColor, float alphaMultiplier) {
    SDL_SetRenderDrawColor(renderer,
                           static_cast<Uint8>(gridColor.r * 255.0f),
                           static_cast<Uint8>(gridColor.g * 255.0f),
                           static_cast<Uint8>(gridColor.b * 255.0f),
                           static_cast<Uint8>(gridColor.a * alphaMultiplier * 255.0f));
}

void Grid::GetEffectiveSpacings(const Camera& camera, float& outMajorSpacing, float& outMinorSpacing) const {
    outMajorSpacing = m_settings->m_baseMajorSpacing;
    const int subdivisions = m_settings->m_subdivisions > 0 ? m_settings->m_subdivisions : 1;

    if (m_settings->m_isDynamic) {
        // Use min/max pixel step from settings with safety clamps
        const float minPixelStep = std::max(16.0f, m_settings->m_minPixelStep);
        const float maxPixelStep = std::max(minPixelStep * 1.5f, m_settings->m_maxPixelStep);
        
        const float zoom = std::max(1e-6f, camera.GetZoom()); // Avoid division by zero

        // Current pixel distance between major grid lines
        float currentPixelStep = outMajorSpacing * zoom;

        // Instead of using a fixed adjustment factor of 10, calculate optimal scaling directly
        if (currentPixelStep < minPixelStep) {
            // We need to increase the spacing to reach at least minPixelStep
            float scaleFactor = minPixelStep / currentPixelStep;
            
            // Find nearest power of 2 scale factor that gets us at or above minPixelStep
            float power = std::ceil(std::log2(scaleFactor));
            scaleFactor = std::pow(2.0f, power);
            
            outMajorSpacing *= scaleFactor;
        }
        else if (currentPixelStep > maxPixelStep) {
            // We need to decrease the spacing to be at most maxPixelStep
            float scaleFactor = maxPixelStep / currentPixelStep;
            
            // Find nearest power of 2 scale factor that gets us at or below maxPixelStep
            float power = std::floor(std::log2(scaleFactor));
            scaleFactor = std::pow(2.0f, power);
            
            outMajorSpacing *= scaleFactor;
        }

        // Safety checks to keep majorSpacing in a reasonable range
        outMajorSpacing = std::max(1e-7f, std::min(1e7f, outMajorSpacing));
    }
    outMinorSpacing = outMajorSpacing / subdivisions;
}

void Grid::GetVisibleWorldBounds(const Camera& camera, const Viewport& viewport, Vec2& outMinWorld, Vec2& outMaxWorld) const {
    // Get screen corners
    Vec2 screenCorners[4] = {
        {static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY())},
        {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY())},
        {static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY() + viewport.GetHeight())},
        {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY() + viewport.GetHeight())}
    };

    // Convert screen corners to world coordinates
    Vec2 worldCorners[4];
    for (int i = 0; i < 4; ++i) {
        worldCorners[i] = viewport.ScreenToWorld(screenCorners[i], camera);
    }

    // Find min/max world coordinates
    outMinWorld = worldCorners[0];
    outMaxWorld = worldCorners[0];
    for (int i = 1; i < 4; ++i) {
        outMinWorld.x = std::min(outMinWorld.x, worldCorners[i].x);
        outMinWorld.y = std::min(outMinWorld.y, worldCorners[i].y);
        outMaxWorld.x = std::max(outMaxWorld.x, worldCorners[i].x);
        outMaxWorld.y = std::max(outMaxWorld.y, worldCorners[i].y);
    }
}

void Grid::DrawGridLines(
    SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, 
    float spacing, const GridColor& color, 
    const Vec2& worldMin, const Vec2& worldMax, 
    bool isMajor, float majorSpacingForAxisCheck) const 
{
    if (spacing <= 1e-6f) return; // Avoid issues with tiny spacing

    ConvertAndSetSDLColor(renderer, color);

    // Vertical lines
    float startX = std::ceil(worldMin.x / spacing) * spacing;
    for (float x = startX; x <= worldMax.x; x += spacing) {
        // Skip if this is an axis line and we are drawing major lines (axis drawn separately with its color)
        if (isMajor && m_settings->m_showAxisLines && std::abs(x) < spacing * 0.1f) {
            continue;
        }
        Vec2 screenP1 = viewport.WorldToScreen({x, worldMin.y}, camera);
        Vec2 screenP2 = viewport.WorldToScreen({x, worldMax.y}, camera);
        SDL_RenderLine(renderer, screenP1.x, screenP1.y, screenP2.x, screenP2.y);
    }

    // Horizontal lines
    float startY = std::ceil(worldMin.y / spacing) * spacing;
    for (float y = startY; y <= worldMax.y; y += spacing) {
        if (isMajor && m_settings->m_showAxisLines && std::abs(y) < spacing * 0.1f) {
            continue;
        }
        Vec2 screenP1 = viewport.WorldToScreen({worldMin.x, y}, camera);
        Vec2 screenP2 = viewport.WorldToScreen({worldMax.x, y}, camera);
        SDL_RenderLine(renderer, screenP1.x, screenP1.y, screenP2.x, screenP2.y);
    }
}

void Grid::DrawGridDots(
    SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, 
    float spacing, const GridColor& color, 
    const Vec2& worldMin, const Vec2& worldMax, 
    bool isMajor, float majorSpacingForAxisCheck) const 
{
    if (spacing <= 1e-6f) return;

    ConvertAndSetSDLColor(renderer, color);

    float startX = std::ceil(worldMin.x / spacing) * spacing;
    float startY = std::ceil(worldMin.y / spacing) * spacing;

    for (float x = startX; x <= worldMax.x; x += spacing) {
        for (float y = startY; y <= worldMax.y; y += spacing) {
            if (isMajor && m_settings->m_showAxisLines && 
                (std::abs(x) < spacing * 0.1f || std::abs(y) < spacing * 0.1f)) {
                // Avoid drawing dots on axis lines if they are drawn separately
                // This logic might need refinement if axes are to be dotted too.
                // For now, skip if either x or y is an axis line for major dots.
                continue; 
            }
            Vec2 screenP = viewport.WorldToScreen({x, y}, camera);
            SDL_RenderPoint(renderer, screenP.x, screenP.y);
        }
    }
}

void Grid::DrawLinesStyle(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, float majorSpacing, float minorSpacing, const Vec2& worldMin, const Vec2& worldMax) const {
    // Draw Minor Lines first, so Major lines can draw over them if perfectly aligned
    if (m_settings->m_subdivisions > 0 && minorSpacing > 1e-6f) {
        DrawGridLines(renderer, camera, viewport, minorSpacing, m_settings->m_minorLineColor, worldMin, worldMax, false, majorSpacing);
    }
    // Draw Major Lines
    DrawGridLines(renderer, camera, viewport, majorSpacing, m_settings->m_majorLineColor, worldMin, worldMax, true, majorSpacing);
}

void Grid::DrawDotsStyle(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, float majorSpacing, float minorSpacing, const Vec2& worldMin, const Vec2& worldMax) const {
    if (m_settings->m_subdivisions > 0 && minorSpacing > 1e-6f) {
        DrawGridDots(renderer, camera, viewport, minorSpacing, m_settings->m_minorLineColor, worldMin, worldMax, false, majorSpacing);
    }
    DrawGridDots(renderer, camera, viewport, majorSpacing, m_settings->m_majorLineColor, worldMin, worldMax, true, majorSpacing);
}

void Grid::DrawAxis(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport, const Vec2& worldMin, const Vec2& worldMax) const {
    if (!m_settings->m_showAxisLines) return;

    // X-Axis (y=0)
    if (0.0f >= worldMin.y && 0.0f <= worldMax.y) {
        ConvertAndSetSDLColor(renderer, m_settings->m_xAxisColor);
        Vec2 screenStart = viewport.WorldToScreen({worldMin.x, 0.0f}, camera);
        Vec2 screenEnd = viewport.WorldToScreen({worldMax.x, 0.0f}, camera);
        SDL_RenderLine(renderer, screenStart.x, screenStart.y, screenEnd.x, screenEnd.y);
    }

    // Y-Axis (x=0)
    if (0.0f >= worldMin.x && 0.0f <= worldMax.x) {
        ConvertAndSetSDLColor(renderer, m_settings->m_yAxisColor);
        Vec2 screenStart = viewport.WorldToScreen({0.0f, worldMin.y}, camera);
        Vec2 screenEnd = viewport.WorldToScreen({0.0f, worldMax.y}, camera);
        SDL_RenderLine(renderer, screenStart.x, screenStart.y, screenEnd.x, screenEnd.y);
    }
}

void Grid::Render(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport) const {
    if (!m_settings || !m_settings->m_visible || !renderer) {
        return;
    }

    // Store original blend mode and set to blend for grid lines (if alpha is used)
    SDL_BlendMode originalBlendMode;
    SDL_GetRenderDrawBlendMode(renderer, &originalBlendMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Clear the grid area with the grid's specific background color
    if (m_settings->m_backgroundColor.a > 0.0f) { // Only clear if the background is not fully transparent
        ConvertAndSetSDLColor(renderer, m_settings->m_backgroundColor); // Use existing helper
        SDL_FRect viewportRect = {
            static_cast<float>(viewport.GetX()), 
            static_cast<float>(viewport.GetY()), 
            static_cast<float>(viewport.GetWidth()), 
            static_cast<float>(viewport.GetHeight())
        };
        SDL_RenderFillRect(renderer, &viewportRect);
    }

    float majorSpacing, minorSpacing;
    GetEffectiveSpacings(camera, majorSpacing, minorSpacing);

    Vec2 worldMin, worldMax;
    GetVisibleWorldBounds(camera, viewport, worldMin, worldMax);

    // Expand bounds slightly to ensure lines at edges are fully drawn if viewport clips them
    // This might be more critical if line drawing itself doesn't extend beyond the viewport.
    // float padding = majorSpacing; // Example padding
    // worldMin.x -= padding; worldMin.y -= padding;
    // worldMax.x += padding; worldMax.y += padding;

    if (m_settings->m_style == GridStyle::LINES) {
        DrawLinesStyle(renderer, camera, viewport, majorSpacing, minorSpacing, worldMin, worldMax);
    } else if (m_settings->m_style == GridStyle::DOTS) {
        DrawDotsStyle(renderer, camera, viewport, majorSpacing, minorSpacing, worldMin, worldMax);
    }

    DrawAxis(renderer, camera, viewport, worldMin, worldMax);
    
    // Restore original blend mode
    SDL_SetRenderDrawBlendMode(renderer, originalBlendMode);
} 
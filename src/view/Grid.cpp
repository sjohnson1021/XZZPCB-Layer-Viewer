#include "view/Grid.hpp"
#include "view/GridSettings.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include <blend2d.h> // Include for BLContext and BLRgba32 etc.
#include <cmath> // For std::floor, std::ceil, std::abs, std::fmod, std::min, std::max
#include <algorithm> // For std::min/max with initializer list
#include <vector>
#include <iostream>

// Define PI if not already available from Camera.cpp or a common math header
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Grid::Grid(std::shared_ptr<GridSettings> settings)
    : m_settings(settings) {}

Grid::~Grid() {}

void Grid::GetEffectiveSpacings(const Camera& camera, float& outMajorSpacing, float& outMinorSpacing) const {
    outMajorSpacing = m_settings->m_baseMajorSpacing;
    const int subdivisions = m_settings->m_subdivisions > 0 ? m_settings->m_subdivisions : 1;

    if (m_settings->m_isDynamic) {
        const float minPixelStep = std::max(16.0f, m_settings->m_minPixelStep);
        const float maxPixelStep = std::max(minPixelStep * 1.5f, m_settings->m_maxPixelStep);
        const float zoom = std::max(1e-6f, camera.GetZoom());
        float currentPixelStep = outMajorSpacing * zoom;

        if (currentPixelStep < minPixelStep) {
            float scaleFactor = minPixelStep / currentPixelStep;
            float power = std::ceil(std::log2(scaleFactor));
            scaleFactor = std::pow(2.0f, power);
            outMajorSpacing *= scaleFactor;
        }
        else if (currentPixelStep > maxPixelStep) {
            float scaleFactor = maxPixelStep / currentPixelStep;
            float power = std::floor(std::log2(scaleFactor));
            scaleFactor = std::pow(2.0f, power);
            outMajorSpacing *= scaleFactor;
        }
        outMajorSpacing = std::max(1e-7f, std::min(1e7f, outMajorSpacing));
    }
    outMinorSpacing = outMajorSpacing / subdivisions;
}

void Grid::GetVisibleWorldBounds(const Camera& camera, const Viewport& viewport, Vec2& outMinWorld, Vec2& outMaxWorld) const {
    Vec2 screenCorners[4] = {
        {static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY())},
        {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY())},
        {static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY() + viewport.GetHeight())},
        {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY() + viewport.GetHeight())}
    };
    Vec2 worldCorners[4];
    for (int i = 0; i < 4; ++i) {
        worldCorners[i] = viewport.ScreenToWorld(screenCorners[i], camera);
    }
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
    BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, 
    float spacing, const GridColor& color, 
    const Vec2& worldMin, const Vec2& worldMax, 
    bool isMajor, float majorSpacingForAxisCheck) const 
{
    if (spacing <= 1e-6f) return;

    try {
        // Set color
        BLRgba32 lineColor(
            static_cast<uint32_t>(color.r * 255.0f),
            static_cast<uint32_t>(color.g * 255.0f),
            static_cast<uint32_t>(color.b * 255.0f),
            static_cast<uint32_t>(color.a * 255.0f)
        );
        
        // Create a single path for all lines
        BLPath linesPath;
        
        // Culling optimization - limit maximum lines per render pass
        const int maxLinesPerAxis = 200;
        
        // Vertical lines
        float startX = std::ceil(worldMin.x / spacing) * spacing;
        float endX = worldMax.x;
        int horizLines = std::ceil((endX - startX) / spacing);
        
        // Horizontal lines
        float startY = std::ceil(worldMin.y / spacing) * spacing;
        float endY = worldMax.y;
        int vertLines = std::ceil((endY - startY) / spacing);
        
        // Total number of lines
        int totalLines = horizLines + vertLines;
        
        // Very dense grid detection (implement pattern-based approach for extremely dense grids)
        const int ultraDenseThreshold = 1000;
        if (totalLines > ultraDenseThreshold) {
            // For extremely dense grids, we can draw a pattern instead
            float scaleFactor = std::max(
                horizLines > 0 ? static_cast<float>(horizLines) / maxLinesPerAxis : 1.0f,
                vertLines > 0 ? static_cast<float>(vertLines) / maxLinesPerAxis : 1.0f
            );
            
            // Round up scaling to the nearest power of 2 for consistent grid appearance
            int power = std::ceil(std::log2(scaleFactor));
            scaleFactor = std::pow(2.0f, power);
            
            // Increase spacing
            spacing *= scaleFactor;
            
            // Recalculate start positions with new spacing
            startX = std::ceil(worldMin.x / spacing) * spacing;
            startY = std::ceil(worldMin.y / spacing) * spacing;
            
            // Recalculate number of lines with new spacing
            horizLines = std::ceil((endX - startX) / spacing);
            vertLines = std::ceil((endY - startY) / spacing);
        }
        
        // Reserve memory for the path (significant optimization)
        // Each line needs 2 points (moveTo + lineTo)
        linesPath.reserve((horizLines + vertLines) * 2);
        
        // Pre-transform all line endpoints at once
        // For vertical lines
        std::vector<Vec2> verticalStartPoints;
        std::vector<Vec2> verticalEndPoints;
        verticalStartPoints.reserve(horizLines);
        verticalEndPoints.reserve(horizLines);
        
        // For horizontal lines
        std::vector<Vec2> horizontalStartPoints;
        std::vector<Vec2> horizontalEndPoints;
        horizontalStartPoints.reserve(vertLines);
        horizontalEndPoints.reserve(vertLines);
        
        // Calculate all screen coordinates for vertical lines
        for (float x = startX; x <= endX; x += spacing) {
            if (isMajor && m_settings->m_showAxisLines && std::abs(x) < spacing * 0.1f) {
                continue; // Skip lines near axis if showing axis lines
            }
            
            Vec2 screenP1 = viewport.WorldToScreen({x, worldMin.y}, camera);
            Vec2 screenP2 = viewport.WorldToScreen({x, worldMax.y}, camera);
            
            if (std::isfinite(screenP1.x) && std::isfinite(screenP1.y) && 
                std::isfinite(screenP2.x) && std::isfinite(screenP2.y)) {
                verticalStartPoints.push_back(screenP1);
                verticalEndPoints.push_back(screenP2);
            }
        }
        
        // Calculate all screen coordinates for horizontal lines
        for (float y = startY; y <= endY; y += spacing) {
            if (isMajor && m_settings->m_showAxisLines && std::abs(y) < spacing * 0.1f) {
                continue; // Skip lines near axis if showing axis lines
            }
            
            Vec2 screenP1 = viewport.WorldToScreen({worldMin.x, y}, camera);
            Vec2 screenP2 = viewport.WorldToScreen({worldMax.x, y}, camera);
            
            if (std::isfinite(screenP1.x) && std::isfinite(screenP1.y) && 
                std::isfinite(screenP2.x) && std::isfinite(screenP2.y)) {
                horizontalStartPoints.push_back(screenP1);
                horizontalEndPoints.push_back(screenP2);
            }
        }
        
        // Add all vertical lines to the path in one batch
        for (size_t i = 0; i < verticalStartPoints.size(); i++) {
            linesPath.moveTo(verticalStartPoints[i].x, verticalStartPoints[i].y);
            linesPath.lineTo(verticalEndPoints[i].x, verticalEndPoints[i].y);
        }
        
        // Add all horizontal lines to the path in one batch
        for (size_t i = 0; i < horizontalStartPoints.size(); i++) {
            linesPath.moveTo(horizontalStartPoints[i].x, horizontalStartPoints[i].y);
            linesPath.lineTo(horizontalEndPoints[i].x, horizontalEndPoints[i].y);
        }
        
        // Draw all lines at once if we have any
        if (!linesPath.empty()) {
            bl_ctx.setStrokeStyle(lineColor);
            bl_ctx.setStrokeWidth(m_settings->m_lineThickness);
            
            // Use a faster composition operation if possible
            BLCompOp savedCompOp = bl_ctx.compOp();
            if (lineColor.a == 255) {
                bl_ctx.setCompOp(BL_COMP_OP_SRC_OVER);
            }
            
            // Stroke the entire path at once
            bl_ctx.strokePath(linesPath);
            
            // Restore composition op if changed
            if (lineColor.a == 255) {
                bl_ctx.setCompOp(savedCompOp);
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Grid::DrawGridLines Exception: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Grid::DrawGridLines: Unknown exception" << std::endl;
    }
}

void Grid::DrawGridDots(
    BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, 
    float spacing, const GridColor& color, 
    const Vec2& worldMin, const Vec2& worldMax, 
    bool isMajor, float majorSpacingForAxisCheck) const 
{
    if (spacing <= 1e-6f) return;

    try {
        // Set color
        BLRgba32 dotColor(
            static_cast<uint32_t>(color.r * 255.0f),
            static_cast<uint32_t>(color.g * 255.0f),
            static_cast<uint32_t>(color.b * 255.0f),
            static_cast<uint32_t>(color.a * 255.0f)
        );
        
        // Define dot radius from settings
        const float dotRadius = m_settings->m_dotRadius;
        
        // Create a path to batch all dots together
        BLPath dotsPath;
        
        // Calculate how many dots we'll potentially draw
        int dotsCount = 0;
        float startX = std::ceil(worldMin.x / spacing) * spacing;
        float startY = std::ceil(worldMin.y / spacing) * spacing;
        float endX = worldMax.x;
        float endY = worldMax.y;
        
        // Culling optimization - limit maximum dots per render pass
        const int maxDotsPerAxis = 200;
        int horizDots = std::ceil((endX - startX) / spacing);
        int vertDots = std::ceil((endY - startY) / spacing);
        
        // If we have too many dots, increase spacing for this render pass
        if (horizDots > maxDotsPerAxis || vertDots > maxDotsPerAxis) {
            float scaleFactor = std::max(
                horizDots > 0 ? static_cast<float>(horizDots) / maxDotsPerAxis : 1.0f,
                vertDots > 0 ? static_cast<float>(vertDots) / maxDotsPerAxis : 1.0f
            );
            
            // Round up scaling to the nearest power of 2 for consistent grid appearance
            int power = std::ceil(std::log2(scaleFactor));
            scaleFactor = std::pow(2.0f, power);
            
            // Increase spacing
            spacing *= scaleFactor;
            
            // Recalculate start positions with new spacing
            startX = std::ceil(worldMin.x / spacing) * spacing;
            startY = std::ceil(worldMin.y / spacing) * spacing;
        }
        
        // Add dots to path
        for (float x = startX; x <= endX; x += spacing) {
            for (float y = startY; y <= endY; y += spacing) {
                if (isMajor && m_settings->m_showAxisLines && 
                    (std::abs(x) < spacing * 0.1f || std::abs(y) < spacing * 0.1f)) {
                    continue; // Skip dots near axis if showing axis lines
                }
                
                Vec2 screenP = viewport.WorldToScreen({x, y}, camera);
                
                if (std::isfinite(screenP.x) && std::isfinite(screenP.y)) {
                    // Safety check: only draw if dot is inside or near viewport
                    if (screenP.x >= -dotRadius && screenP.x <= viewport.GetWidth() + dotRadius &&
                        screenP.y >= -dotRadius && screenP.y <= viewport.GetHeight() + dotRadius) {
                        // Add circle to the path (much faster than drawing individually)
                        dotsPath.addCircle(BLCircle(screenP.x, screenP.y, dotRadius));
                        dotsCount++;
                    }
                }
            }
        }
        
        // Draw all dots at once if we have any
        if (dotsCount > 0) {
            bl_ctx.setFillStyle(dotColor);
            bl_ctx.fillPath(dotsPath);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Grid::DrawGridDots Exception: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Grid::DrawGridDots: Unknown exception" << std::endl;
    }
}

void Grid::DrawLinesStyle(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, float majorSpacing, float minorSpacing, const Vec2& worldMin, const Vec2& worldMax) const {
    if (m_settings->m_subdivisions > 0 && minorSpacing > 1e-6f) {
        DrawGridLines(bl_ctx, camera, viewport, minorSpacing, m_settings->m_minorLineColor, worldMin, worldMax, false, majorSpacing);
    }
    DrawGridLines(bl_ctx, camera, viewport, majorSpacing, m_settings->m_majorLineColor, worldMin, worldMax, true, majorSpacing);
}

void Grid::DrawDotsStyle(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, float majorSpacing, float minorSpacing, const Vec2& worldMin, const Vec2& worldMax) const {
    if (m_settings->m_subdivisions > 0 && minorSpacing > 1e-6f) {
        DrawGridDots(bl_ctx, camera, viewport, minorSpacing, m_settings->m_minorLineColor, worldMin, worldMax, false, majorSpacing);
    }
    DrawGridDots(bl_ctx, camera, viewport, majorSpacing, m_settings->m_majorLineColor, worldMin, worldMax, true, majorSpacing);
}

void Grid::DrawAxis(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, const Vec2& worldMin, const Vec2& worldMax) const {
    if (!m_settings->m_showAxisLines) return;

    try {
        bl_ctx.setStrokeWidth(m_settings->m_axisLineThickness);

        // X-Axis (y=0)
        if (0.0f >= worldMin.y && 0.0f <= worldMax.y) {
            BLRgba32 xAxisColor(
                static_cast<uint32_t>(m_settings->m_xAxisColor.r * 255.0f),
                static_cast<uint32_t>(m_settings->m_xAxisColor.g * 255.0f),
                static_cast<uint32_t>(m_settings->m_xAxisColor.b * 255.0f),
                static_cast<uint32_t>(m_settings->m_xAxisColor.a * 255.0f)
            );
            bl_ctx.setStrokeStyle(xAxisColor);
            
            Vec2 screenStart = viewport.WorldToScreen({worldMin.x, 0.0f}, camera);
            Vec2 screenEnd = viewport.WorldToScreen({worldMax.x, 0.0f}, camera);
            
            if (std::isfinite(screenStart.x) && std::isfinite(screenStart.y) && 
                std::isfinite(screenEnd.x) && std::isfinite(screenEnd.y)) {
                bl_ctx.strokeLine(screenStart.x, screenStart.y, screenEnd.x, screenEnd.y);
            }
        }

        // Y-Axis (x=0)
        if (0.0f >= worldMin.x && 0.0f <= worldMax.x) {
            BLRgba32 yAxisColor(
                static_cast<uint32_t>(m_settings->m_yAxisColor.r * 255.0f),
                static_cast<uint32_t>(m_settings->m_yAxisColor.g * 255.0f),
                static_cast<uint32_t>(m_settings->m_yAxisColor.b * 255.0f),
                static_cast<uint32_t>(m_settings->m_yAxisColor.a * 255.0f)
            );
            bl_ctx.setStrokeStyle(yAxisColor);
            
            Vec2 screenStart = viewport.WorldToScreen({0.0f, worldMin.y}, camera);
            Vec2 screenEnd = viewport.WorldToScreen({0.0f, worldMax.y}, camera);
            
            if (std::isfinite(screenStart.x) && std::isfinite(screenStart.y) && 
                std::isfinite(screenEnd.x) && std::isfinite(screenEnd.y)) {
                bl_ctx.strokeLine(screenStart.x, screenStart.y, screenEnd.x, screenEnd.y);
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Grid::DrawAxis Exception: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Grid::DrawAxis: Unknown exception" << std::endl;
    }
}

void Grid::Render(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport) const {
    if (!m_settings || !m_settings->m_visible) {
        return;
    }

    try {
        bl_ctx.save();

        // Clear the grid area with the grid's specific background color
        if (m_settings->m_backgroundColor.a > 0.0f) {
            bl_ctx.setFillStyle(BLRgba32(
                static_cast<uint32_t>(m_settings->m_backgroundColor.r * 255.0f),
                static_cast<uint32_t>(m_settings->m_backgroundColor.g * 255.0f),
                static_cast<uint32_t>(m_settings->m_backgroundColor.b * 255.0f),
                static_cast<uint32_t>(m_settings->m_backgroundColor.a * 255.0f)
            ));

            // Filled rectangle with the background color
            bl_ctx.fillRect(BLRect(0, 0, viewport.GetWidth(), viewport.GetHeight()));
        }

        float majorSpacing, minorSpacing;
        GetEffectiveSpacings(camera, majorSpacing, minorSpacing);

        Vec2 worldMin, worldMax;
        GetVisibleWorldBounds(camera, viewport, worldMin, worldMax);

        // Clip to viewport bounds to prevent drawing outside
        bl_ctx.clipToRect(BLRect(0, 0, viewport.GetWidth(), viewport.GetHeight()));

        if (m_settings->m_style == GridStyle::LINES) {
            DrawLinesStyle(bl_ctx, camera, viewport, majorSpacing, minorSpacing, worldMin, worldMax);
        } else if (m_settings->m_style == GridStyle::DOTS) {
            DrawDotsStyle(bl_ctx, camera, viewport, majorSpacing, minorSpacing, worldMin, worldMax);
        }

        DrawAxis(bl_ctx, camera, viewport, worldMin, worldMax);

        // Restore context state to not affect future drawing
        bl_ctx.restore();
    }
    catch (const std::exception& e) {
        std::cerr << "Grid::Render Exception: " << e.what() << std::endl;
        // Try to restore context state if possible
        try { bl_ctx.restore(); } catch (...) {}
    }
    catch (...) {
        std::cerr << "Grid::Render: Unknown exception" << std::endl;
        // Try to restore context state if possible
        try { bl_ctx.restore(); } catch (...) {}
    }
} 
} 
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

void Grid::GetEffectiveSpacings(
    const Camera& camera, 
    float& outMajorSpacing_world, 
    float& outMinorSpacing_world,
    int& outEffectiveSubdivisions
) const {
    // Rule 0: Input Sanitization (from m_settings)
    float baseMajorSpacing_world = m_settings->m_baseMajorSpacing;
    if (baseMajorSpacing_world <= 1e-6f) baseMajorSpacing_world = 1.0f; // Ensure positive base

    int base_subdivisions_count = std::max(1, m_settings->m_subdivisions);
    
    // Ensure minPixelStep is at least 1.0f, as per Rule 0.
    float minPixelStep_screen = std::max(1.0f, m_settings->m_minPixelStep);
    // Ensure maxPixelStep is greater than minPixelStep, as per Rule 0.
    float maxPixelStep_screen = std::max(minPixelStep_screen * 1.5f, m_settings->m_maxPixelStep);
    
    const float currentZoom = std::max(1e-6f, camera.GetZoom()); // Ensure currentZoom > 0

    // Initialize outputs
    outMajorSpacing_world = baseMajorSpacing_world;
    outEffectiveSubdivisions = base_subdivisions_count;

    if (m_settings->m_isDynamic) {
        // Rule 1: Dynamic Major Line Spacing (World Units)
        float currentMajorSpacingOnScreen = outMajorSpacing_world * currentZoom;

        if (currentMajorSpacingOnScreen < minPixelStep_screen) {
            float scaleFactor = minPixelStep_screen / currentMajorSpacingOnScreen;
            float power = std::ceil(std::log2(scaleFactor));
            outMajorSpacing_world *= std::pow(2.0f, power);
        }
        else if (currentMajorSpacingOnScreen > maxPixelStep_screen) {
            float scaleFactor = maxPixelStep_screen / currentMajorSpacingOnScreen;
            float power = std::floor(std::log2(scaleFactor));
            outMajorSpacing_world *= std::pow(2.0f, power);
        }
        // Clamp effectiveMajorSpacing_world within reasonable world unit bounds (e.g., 1e-7 to 1e7)
        outMajorSpacing_world = std::max(1e-7f, std::min(1e7f, outMajorSpacing_world));

        // Rule 2: Dynamic Minor Line Density (Effective Subdivision Count)
        if (base_subdivisions_count > 1) { // Only adjust if there are subdivisions to begin with
            float potentialMinorWorldSpacing_via_base_sub = outMajorSpacing_world / base_subdivisions_count;
            float potentialMinorScreenSpacing_via_base_sub = potentialMinorWorldSpacing_via_base_sub * currentZoom;

            if (potentialMinorScreenSpacing_via_base_sub < minPixelStep_screen) {
                double sparsityFactor_double = 1.0;
                // Ensure potentialMinorScreenSpacing_via_base_sub is positive before division
                if (potentialMinorScreenSpacing_via_base_sub > 1e-9f) { 
                    sparsityFactor_double = std::ceil(static_cast<double>(minPixelStep_screen) / potentialMinorScreenSpacing_via_base_sub);
                } else { 
                    // If potential screen spacing is tiny/zero, make sparsity high to reduce subdivisions to 1
                    sparsityFactor_double = static_cast<double>(base_subdivisions_count) + 1.0; 
                }
                
                int sparsityFactor = static_cast<int>(sparsityFactor_double);
                // Ensure sparsityFactor is at least 1 to avoid division by zero or issues with floor
                if (sparsityFactor <= 0) sparsityFactor = 1; 

                outEffectiveSubdivisions = static_cast<int>(std::floor(static_cast<double>(base_subdivisions_count) / sparsityFactor));
                outEffectiveSubdivisions = std::max(1, outEffectiveSubdivisions); // Ensure at least 1 subdivision
            } else {
                outEffectiveSubdivisions = base_subdivisions_count;
            }
        } else {
            outEffectiveSubdivisions = 1; // No subdivisions if base_subdivisions_count was 1
        }
    } else {
        // Rule 3: Static Mode Parameterization
        outMajorSpacing_world = baseMajorSpacing_world;
        outEffectiveSubdivisions = base_subdivisions_count;
    }

    // Final calculation for outMinorSpacing_world based on effective subdivisions
    // outEffectiveSubdivisions is guaranteed to be >= 1 here.
    outMinorSpacing_world = outMajorSpacing_world / outEffectiveSubdivisions;
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
        BLRgba32 lineColor(
            static_cast<uint32_t>(color.r * 255.0f),
            static_cast<uint32_t>(color.g * 255.0f),
            static_cast<uint32_t>(color.b * 255.0f),
            static_cast<uint32_t>(color.a * 255.0f)
        );
        
        BLPath linesPath;

        long long i_start_x = static_cast<long long>(std::ceil(worldMin.x / spacing));
        long long i_end_x = static_cast<long long>(std::floor(worldMax.x / spacing));
        int reserveHorizLines = (i_end_x >= i_start_x) ? static_cast<int>(i_end_x - i_start_x + 1) : 0;

        long long i_start_y = static_cast<long long>(std::ceil(worldMin.y / spacing));
        long long i_end_y = static_cast<long long>(std::floor(worldMax.y / spacing));
        int reserveVertLines = (i_end_y >= i_start_y) ? static_cast<int>(i_end_y - i_start_y + 1) : 0;
        
        const int max_reserve_per_axis = 10000;
        linesPath.reserve((std::min(reserveHorizLines, max_reserve_per_axis) + std::min(reserveVertLines, max_reserve_per_axis)) * 2);
        
        std::vector<Vec2> verticalStartPoints;
        std::vector<Vec2> verticalEndPoints;
        verticalStartPoints.reserve(std::min(reserveHorizLines, max_reserve_per_axis));
        verticalEndPoints.reserve(std::min(reserveHorizLines, max_reserve_per_axis));
        
        std::vector<Vec2> horizontalStartPoints;
        std::vector<Vec2> horizontalEndPoints;
        horizontalStartPoints.reserve(std::min(reserveVertLines, max_reserve_per_axis));
        horizontalEndPoints.reserve(std::min(reserveVertLines, max_reserve_per_axis));
        
        for (long long i = i_start_x; i <= i_end_x; ++i) {
            float x = static_cast<float>(i) * spacing;
            if (isMajor && m_settings->m_showAxisLines && std::abs(x) < majorSpacingForAxisCheck * 0.1f) {
                continue;
            }
            Vec2 screenP1 = viewport.WorldToScreen({x, worldMin.y}, camera);
            Vec2 screenP2 = viewport.WorldToScreen({x, worldMax.y}, camera);
            if (std::isfinite(screenP1.x) && std::isfinite(screenP1.y) && 
                std::isfinite(screenP2.x) && std::isfinite(screenP2.y)) {
                verticalStartPoints.push_back(screenP1);
                verticalEndPoints.push_back(screenP2);
            }
        }
        
        for (long long i = i_start_y; i <= i_end_y; ++i) {
            float y = static_cast<float>(i) * spacing;
            if (isMajor && m_settings->m_showAxisLines && std::abs(y) < majorSpacingForAxisCheck * 0.1f) {
                continue;
            }
            Vec2 screenP1 = viewport.WorldToScreen({worldMin.x, y}, camera);
            Vec2 screenP2 = viewport.WorldToScreen({worldMax.x, y}, camera);
            if (std::isfinite(screenP1.x) && std::isfinite(screenP1.y) && 
                std::isfinite(screenP2.x) && std::isfinite(screenP2.y)) {
                horizontalStartPoints.push_back(screenP1);
                horizontalEndPoints.push_back(screenP2);
            }
        }
        
        for (size_t i = 0; i < verticalStartPoints.size(); i++) {
            linesPath.moveTo(verticalStartPoints[i].x, verticalStartPoints[i].y);
            linesPath.lineTo(verticalEndPoints[i].x, verticalEndPoints[i].y);
        }
        
        for (size_t i = 0; i < horizontalStartPoints.size(); i++) {
            linesPath.moveTo(horizontalStartPoints[i].x, horizontalStartPoints[i].y);
            linesPath.lineTo(horizontalEndPoints[i].x, horizontalEndPoints[i].y);
        }
        
        if (!linesPath.empty()) {
            bl_ctx.setStrokeStyle(lineColor);
            bl_ctx.setStrokeWidth(m_settings->m_lineThickness);
            BLCompOp savedCompOp = bl_ctx.compOp();
            if (lineColor.a() == 255) {
                bl_ctx.setCompOp(BL_COMP_OP_SRC_OVER);
            }
            bl_ctx.strokePath(linesPath);
            if (lineColor.a() == 255) {
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
        BLRgba32 dotColor(
            static_cast<uint32_t>(color.r * 255.0f),
            static_cast<uint32_t>(color.g * 255.0f),
            static_cast<uint32_t>(color.b * 255.0f),
            static_cast<uint32_t>(color.a * 255.0f)
        );
        
        const float dotRadius = m_settings->m_dotRadius;
        BLPath dotsPath;
        int dotsCount = 0;

        long long i_start_x = static_cast<long long>(std::ceil(worldMin.x / spacing));
        long long i_end_x = static_cast<long long>(std::floor(worldMax.x / spacing));
        long long i_start_y = static_cast<long long>(std::ceil(worldMin.y / spacing));
        long long i_end_y = static_cast<long long>(std::floor(worldMax.y / spacing));

        int num_potential_dots_x = (i_end_x >= i_start_x) ? static_cast<int>(i_end_x - i_start_x + 1) : 0;
        int num_potential_dots_y = (i_end_y >= i_start_y) ? static_cast<int>(i_end_y - i_start_y + 1) : 0;
        
        const int max_reserve_total_dots = 100000; // Arbitrary large number for total dots
        if (num_potential_dots_x > 0 && num_potential_dots_y > 0) {
            if (static_cast<double>(num_potential_dots_x) * num_potential_dots_y < max_reserve_total_dots) {
                 dotsPath.reserve(num_potential_dots_x * num_potential_dots_y);
            } else {
                 dotsPath.reserve(max_reserve_total_dots); // Cap reservation
            }
        }

        for (long long i_x = i_start_x; i_x <= i_end_x; ++i_x) {
            float x = static_cast<float>(i_x) * spacing;
            for (long long i_y = i_start_y; i_y <= i_end_y; ++i_y) {
                float y = static_cast<float>(i_y) * spacing;
                
                if (isMajor && m_settings->m_showAxisLines && 
                    (std::abs(x) < majorSpacingForAxisCheck * 0.1f || std::abs(y) < majorSpacingForAxisCheck * 0.1f)) {
                    continue;
                }
                
                Vec2 screenP = viewport.WorldToScreen({x, y}, camera);
                
                if (std::isfinite(screenP.x) && std::isfinite(screenP.y)) {
                    if (screenP.x >= -dotRadius && screenP.x <= viewport.GetWidth() + dotRadius &&
                        screenP.y >= -dotRadius && screenP.y <= viewport.GetHeight() + dotRadius) {
                        if (dotsCount < max_reserve_total_dots) {
                            dotsPath.addCircle(BLCircle(screenP.x, screenP.y, dotRadius));
                            dotsCount++;
                        }
                    }
                }
            }
        }
        
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

void Grid::DrawLinesStyle(
    BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, 
    float majorSpacing, float minorSpacing, 
    const Vec2& worldMin, const Vec2& worldMax,
    bool actuallyDrawMajorLines,
    bool actuallyDrawMinorLines
) const {
    if (actuallyDrawMinorLines) {
        if (std::abs(minorSpacing - majorSpacing) > 1e-6f && minorSpacing > 1e-6f) { 
             DrawGridLines(bl_ctx, camera, viewport, minorSpacing, m_settings->m_minorLineColor, worldMin, worldMax, false, majorSpacing);
        }
    }
    if (actuallyDrawMajorLines) {
        DrawGridLines(bl_ctx, camera, viewport, majorSpacing, m_settings->m_majorLineColor, worldMin, worldMax, true, majorSpacing);
    }
}

void Grid::DrawDotsStyle(
    BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, 
    float majorSpacing, float minorSpacing, 
    const Vec2& worldMin, const Vec2& worldMax,
    bool actuallyDrawMajorDots,
    bool actuallyDrawMinorDots
) const {
    if (actuallyDrawMinorDots) {
        if (std::abs(minorSpacing - majorSpacing) > 1e-6f && minorSpacing > 1e-6f) {
            DrawGridDots(bl_ctx, camera, viewport, minorSpacing, m_settings->m_minorLineColor, worldMin, worldMax, false, majorSpacing);
        }
    }
    if (actuallyDrawMajorDots) {
        DrawGridDots(bl_ctx, camera, viewport, majorSpacing, m_settings->m_majorLineColor, worldMin, worldMax, true, majorSpacing);
    }
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

        if (m_settings->m_backgroundColor.a > 0.0f) {
            bl_ctx.setFillStyle(BLRgba32(
                static_cast<uint32_t>(m_settings->m_backgroundColor.r * 255.0f),
                static_cast<uint32_t>(m_settings->m_backgroundColor.g * 255.0f),
                static_cast<uint32_t>(m_settings->m_backgroundColor.b * 255.0f),
                static_cast<uint32_t>(m_settings->m_backgroundColor.a * 255.0f)
            ));
            bl_ctx.fillRect(BLRect(0, 0, viewport.GetWidth(), viewport.GetHeight()));
        }

        float effMajorSpacing_world, effMinorSpacing_world;
        int effectiveSubdivisions;
        GetEffectiveSpacings(camera, effMajorSpacing_world, effMinorSpacing_world, effectiveSubdivisions);

        Vec2 worldMin, worldMax;
        GetVisibleWorldBounds(camera, viewport, worldMin, worldMax);

        float culledMajorSpacing = effMajorSpacing_world;
        float culledMinorSpacing = effMinorSpacing_world;
        
        float minPixelStep_setting = std::max(1.0f, m_settings->m_minPixelStep);

        // Rule 5 (modified) for Minor Elements
        bool actuallyDrawMinorElements = false;
        if (effectiveSubdivisions > 1) {
            float currentMinorScreenSpacing = culledMinorSpacing * camera.GetZoom();
            if (currentMinorScreenSpacing >= minPixelStep_setting) {
                actuallyDrawMinorElements = true;
            }
        }

        // New: Rule 5 equivalent for Major Elements (especially for static mode)
        bool actuallyDrawMajorElements = false;
        float currentMajorScreenSpacing = culledMajorSpacing * camera.GetZoom();
        if (currentMajorScreenSpacing >= minPixelStep_setting) {
            actuallyDrawMajorElements = true;
        }

        bl_ctx.clipToRect(BLRect(0, 0, viewport.GetWidth(), viewport.GetHeight()));

        if (m_settings->m_style == GridStyle::LINES) {
            DrawLinesStyle(bl_ctx, camera, viewport, culledMajorSpacing, culledMinorSpacing, worldMin, worldMax, actuallyDrawMajorElements, actuallyDrawMinorElements);
        } else if (m_settings->m_style == GridStyle::DOTS) {
            DrawDotsStyle(bl_ctx, camera, viewport, culledMajorSpacing, culledMinorSpacing, worldMin, worldMax, actuallyDrawMajorElements, actuallyDrawMinorElements);
        }

        // Axis lines are drawn independently of this new major/minor culling for now.
        // They have their own m_showAxisLines setting.
        DrawAxis(bl_ctx, camera, viewport, worldMin, worldMax);

        bl_ctx.restore();
    }
    catch (const std::exception& e) {
        std::cerr << "Grid::Render Exception: " << e.what() << std::endl;
        try { bl_ctx.restore(); } catch (...) {}
    }
    catch (...) {
        std::cerr << "Grid::Render: Unknown exception" << std::endl;
        try { bl_ctx.restore(); } catch (...) {}
    }
} 
 
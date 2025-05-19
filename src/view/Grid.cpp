#include "view/Grid.hpp"
#include "view/GridSettings.hpp"
#include "view/Camera.hpp"
#include "view/Viewport.hpp"
#include <blend2d.h> // Include for BLContext and BLRgba32 etc.
#include <cmath> // For std::floor, std::ceil, std::abs, std::fmod, std::min, std::max
#include <algorithm> // For std::min/max with initializer list
#include <vector>
#include <iostream>
#include <limits> // For std::numeric_limits
#include <string> // For string formatting in measurement readout
#include <sstream> // For string streams
#include <iomanip> // For output formatting

// Define PI if not already available from Camera.cpp or a common math header
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Grid::Grid(std::shared_ptr<GridSettings> settings)
    : m_settings(settings) {}

Grid::~Grid() {}

void Grid::GetNiceUnitFactors(std::vector<float>& outFactors) const {
    if (m_settings->m_unitSystem == GridUnitSystem::METRIC) {
        // Metric: 1, 2, 5, 10 series
        outFactors = {1.0f, 2.0f, 5.0f};
    } else {
        // Imperial: Handle both decimal inches and fractions
        // For decimal: 0.1, 0.25, 0.5, 1.0
        // For fractions: 1/16, 1/8, 1/4, 1/2, 1
        outFactors = {0.0625f, 0.125f, 0.25f, 0.5f, 1.0f};
    }
}

void Grid::GetEffectiveSpacings(
    const Camera& camera, 
    float& outMajorSpacing_world, 
    float& outMinorSpacing_world,
    int& outEffectiveSubdivisions
) const {
    // --- 0. Input Sanitization & Defaults ---
    float baseMajorSpacing_world = m_settings->m_baseMajorSpacing;
    // Ensure positive base spacing, defaulting to appropriate unit-specific value if too small
    if (baseMajorSpacing_world <= 1e-6f) {
        baseMajorSpacing_world = (m_settings->m_unitSystem == GridUnitSystem::METRIC) ? 10.0f : 0.5f;
    }

    // Get appropriate subdivisions based on unit system if none specified
    int baseSubdivisions = std::max(1, m_settings->m_subdivisions);
    if (baseSubdivisions <= 1) {
        baseSubdivisions = (m_settings->m_unitSystem == GridUnitSystem::METRIC) ? 10 : 4;
    }
    
    const float minPxStep = std::max(1.0f, m_settings->m_minPixelStep);
    const float maxPxStep = std::max(minPxStep * 1.5f, m_settings->m_maxPixelStep);
    const float zoom = std::max(1e-6f, camera.GetZoom());

    // Initialize outputs
    outMajorSpacing_world = baseMajorSpacing_world;
    outEffectiveSubdivisions = baseSubdivisions;

    // --- 1. Dynamic Spacing Mode ---
    if (m_settings->m_isDynamic) {
        // 1a. Determine Ideal Major Spacing based on zoom and pixel steps
        float currentMajorScreenPx = outMajorSpacing_world * zoom;

        if (currentMajorScreenPx < minPxStep || currentMajorScreenPx > maxPxStep) {
            float bestFitMajorSpacing = outMajorSpacing_world;
            float smallestDiff = std::numeric_limits<float>::max();
            float targetScreenPx = (minPxStep + maxPxStep) / 2.0f; // Aim for middle of the range

            // Target world spacing that would make currentMajorScreenPx == targetScreenPx
            float targetIdealWorldSpacing = targetScreenPx / zoom;
            if (targetIdealWorldSpacing < 1e-7f) targetIdealWorldSpacing = 1e-7f; // Prevent log10 of zero/negative

            // Determine appropriate base scale using logarithmic approach based on unit system
            float logBase = 10.0f; // For metric system
            if (m_settings->m_unitSystem == GridUnitSystem::IMPERIAL) {
                logBase = 2.0f; // For imperial fractions (powers of 2)
            }
            
            float baseScale = std::pow(10.0f, std::floor(std::log10(targetIdealWorldSpacing)));
            
            // Get "nice" factors appropriate for the current unit system
            std::vector<float> niceFactors;
            GetNiceUnitFactors(niceFactors);

            // Try different powers around the base scale
            for (int i = -2; i <= 2; ++i) {
                float powerOf10 = std::pow(10.0f, i);
                for (float factor : niceFactors) {
                    float candidateSpacing_world = baseScale * powerOf10 * factor;
                    if (candidateSpacing_world < 1e-6f) continue; // Avoid too small spacing

                    float candidateScreenPx = candidateSpacing_world * zoom;
                    if (candidateScreenPx >= minPxStep && candidateScreenPx <= maxPxStep) {
                        float diff = std::abs(candidateScreenPx - targetScreenPx);
                        if (diff < smallestDiff) {
                            smallestDiff = diff;
                            bestFitMajorSpacing = candidateSpacing_world;
                        }
                    }
                }
            }
            
            // If no "nice number" fit was found, use power-of-2 scaling as fallback
            if (smallestDiff == std::numeric_limits<float>::max()) {
                if (currentMajorScreenPx < minPxStep) {
                    float scale = minPxStep / currentMajorScreenPx;
                    bestFitMajorSpacing = outMajorSpacing_world * std::pow(2.0f, std::ceil(std::log2(scale)));
                } else { // currentMajorScreenPx > maxPxStep
                    float scale = maxPxStep / currentMajorScreenPx;
                    if (scale > 0) {
                        bestFitMajorSpacing = outMajorSpacing_world * std::pow(2.0f, std::floor(std::log2(scale)));
                    }
                }
            }
            outMajorSpacing_world = bestFitMajorSpacing;
        }
        
        // Clamp to reasonable world unit bounds
        outMajorSpacing_world = std::max(1e-7f, std::min(1e7f, outMajorSpacing_world));

        // 1b. Determine Effective Subdivisions based on the new outMajorSpacing_world
        if (baseSubdivisions > 1) {
            float potentialMinorWorldSpacing = outMajorSpacing_world / baseSubdivisions;
            float potentialMinorScreenPx = potentialMinorWorldSpacing * zoom;

            if (potentialMinorScreenPx < minPxStep) {
                // Try to find appropriate subdivision count based on unit system
                int newSubdivisions = baseSubdivisions;
                
                if (m_settings->m_unitSystem == GridUnitSystem::METRIC) {
                    // For metric, prefer 10, 5, 2, 1
                    while (newSubdivisions > 1 && (outMajorSpacing_world / newSubdivisions) * zoom < minPxStep) {
                        if (newSubdivisions == 10) newSubdivisions = 5;
                        else if (newSubdivisions == 5) newSubdivisions = 2;
                        else if (newSubdivisions == 2) newSubdivisions = 1;
                        else newSubdivisions = 1; // Fallback
                    }
                } else {
                    // For imperial, prefer 8, 4, 2, 1 (power of 2 divisions)
                    while (newSubdivisions > 1 && (outMajorSpacing_world / newSubdivisions) * zoom < minPxStep) {
                        if (newSubdivisions == 16) newSubdivisions = 8;
                        else if (newSubdivisions == 8) newSubdivisions = 4;
                        else if (newSubdivisions == 4) newSubdivisions = 2;
                        else if (newSubdivisions == 2) newSubdivisions = 1;
                        else newSubdivisions = 1; // Fallback
                    }
                }
                
                outEffectiveSubdivisions = std::max(1, newSubdivisions);
            } else {
                outEffectiveSubdivisions = baseSubdivisions;
            }
        } else {
            outEffectiveSubdivisions = 1;
        }
    }
    // --- 2. Static Spacing Mode ---
    else {
        outMajorSpacing_world = baseMajorSpacing_world;
        outEffectiveSubdivisions = baseSubdivisions;
    }

    // --- 3. Final Minor Spacing Calculation ---
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

void Grid::EnforceRenderingLimits(
    const Camera& camera, const Viewport& viewport,
    float spacing, const Vec2& worldMin, const Vec2& worldMax, 
    int& outEstimatedLineCount, bool& outShouldRender) const
{
    if (spacing <= 1e-6f) {
        outEstimatedLineCount = 0;
        outShouldRender = false;
        return;
    }

    // Calculate how many horizontal and vertical lines would be drawn
    long long i_start_x = static_cast<long long>(std::ceil(worldMin.x / spacing));
    long long i_end_x = static_cast<long long>(std::floor(worldMax.x / spacing));
    int numHorizontalLines = (i_end_x >= i_start_x) ? static_cast<int>(i_end_x - i_start_x + 1) : 0;

    long long i_start_y = static_cast<long long>(std::ceil(worldMin.y / spacing));
    long long i_end_y = static_cast<long long>(std::floor(worldMax.y / spacing));
    int numVerticalLines = (i_end_y >= i_start_y) ? static_cast<int>(i_end_y - i_start_y + 1) : 0;

    // Total number of lines
    outEstimatedLineCount = numHorizontalLines + numVerticalLines;
    
    // For dots, the estimate would be numHorizontalLines * numVerticalLines
    // We'll use the same mechanism for both since dots also require two nested loops
    int dotEstimate = numHorizontalLines * numVerticalLines;

    // Determine if we should render based on limits
    int maxRenderableItems;
    if (m_settings->m_style == GridStyle::LINES) {
        maxRenderableItems = m_settings->MAX_RENDERABLE_LINES;
        outShouldRender = (outEstimatedLineCount <= maxRenderableItems);
    } else { // DOTS
        maxRenderableItems = m_settings->MAX_RENDERABLE_DOTS;
        outShouldRender = (dotEstimate <= maxRenderableItems);
    }
}

Grid::GridMeasurementInfo Grid::GetMeasurementInfo(const Camera& camera, const Viewport& viewport) const {
    GridMeasurementInfo info;
    
    // Get current effective spacings
    GetEffectiveSpacings(camera, info.majorSpacing, info.minorSpacing, info.subdivisions);
    
    // Determine visibility based on screen pixel step
    const float currentZoom = camera.GetZoom();
    const float minPxStep = std::max(1.0f, m_settings->m_minPixelStep);
    
    float majorScreenPx = info.majorSpacing * currentZoom;
    info.majorLinesVisible = (majorScreenPx >= minPxStep);
    
    float minorScreenPx = info.minorSpacing * currentZoom;
    info.minorLinesVisible = info.majorLinesVisible && 
                             info.subdivisions > 1 && 
                             (minorScreenPx >= minPxStep) && 
                             (std::abs(info.majorSpacing - info.minorSpacing) > 1e-6f) && 
                             (info.minorSpacing > 1e-6f);
    
    // Get unit string
    info.unitString = m_settings->UnitToString();
    
    return info;
}

void Grid::RenderMeasurementReadout(BLContext& bl_ctx, const Viewport& viewport, const GridMeasurementInfo& info) const {
    if (!m_settings->m_showMeasurementReadout) {
        return;
    }

    // Create the measurement text
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3); // Display up to 3 decimal places
    
    // Format major spacing
    if (info.majorLinesVisible) {
        ss << "Major: " << info.majorSpacing << info.unitString;
    } else {
        ss << "Major: Hidden";
    }
    
    // Format minor spacing
    ss << " | ";
    if (info.minorLinesVisible) {
        ss << "Minor: " << info.minorSpacing << info.unitString;
    } else {
        ss << "Minor: Hidden";
    }
    
    std::string readoutText = ss.str();
    
    // Setup text position and size
    const int padding = 10;
    const float x = static_cast<float>(viewport.GetX() + padding);
    const float y = static_cast<float>(viewport.GetY() + viewport.GetHeight() - padding - 20);
    
    // Estimate text dimensions based on character count (rough approximation)
    // Average character width is about 8 pixels in a standard font at ~11px size
    float charWidth = 8.0f;
    float textWidth = static_cast<float>(readoutText.length()) * charWidth;
    float textHeight = 16.0f; // Typical text height with some padding
    
    // Draw background with semi-transparent black
    const float boxX = x - 5.0f;
    const float boxY = y - textHeight + 5.0f;
    const float boxWidth = textWidth + 10.0f; 
    const float boxHeight = textHeight + 10.0f;
    
    bl_ctx.setFillStyle(BLRgba32(0, 0, 0, 196));
    bl_ctx.fillRect(BLRect(boxX, boxY, boxWidth, boxHeight));
    
    // Create a font from the specified font file
    static BLFontFace fontFace;
    static bool fontFaceInitialized = false;
    static bool fontFaceLoadFailed = false;
    
    // Try to load the font once and cache the result
    if (!fontFaceInitialized) {
        fontFaceInitialized = true;
        
        // Use the project's font file
        BLResult faceResult = fontFace.createFromFile("assets/fonts/Nippo-Light.otf");
        if (faceResult != BL_SUCCESS) {
            // Try with full path if relative path fails
            faceResult = fontFace.createFromFile("/home/seanj/Documents/Code/XZZPCB-Layer-Viewer/assets/fonts/Nippo-Light.otf");
            if (faceResult != BL_SUCCESS) {
                fontFaceLoadFailed = true;
                std::cerr << "Failed to load font file for grid measurement readout" << std::endl;
            }
        }
    }
    
    if (!fontFaceLoadFailed) {
        BLFont font;
        BLResult fontResult = font.createFromFace(fontFace, 11.0f);
        
        if (fontResult == BL_SUCCESS) {
            // Draw text with solid white if font creation succeeded
            bl_ctx.setFillStyle(BLRgba32(255, 255, 255, 255));
            bl_ctx.fillUtf8Text(BLPoint(x, y), font, readoutText.c_str(), readoutText.length());
        } else {
            // Fallback if font creation failed - draw simple text indicator
            bl_ctx.setFillStyle(BLRgba32(255, 255, 255, 255));
            for (size_t i = 0; i < readoutText.length(); i++) {
                bl_ctx.fillRect(BLRect(x + i * charWidth, y - textHeight/2, 
                                      (readoutText[i] == ' ') ? charWidth/4 : charWidth/2, 1));
            }
        }
    } else {
        // Fallback if font face creation failed - draw simple text indicator
        bl_ctx.setFillStyle(BLRgba32(255, 255, 255, 255));
        for (size_t i = 0; i < readoutText.length(); i++) {
            bl_ctx.fillRect(BLRect(x + i * charWidth, y - textHeight/2, 
                                  (readoutText[i] == ' ') ? charWidth/4 : charWidth/2, 1));
        }
    }
}

void Grid::DrawGridLines(
    BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, 
    float spacing, const GridColor& color,
    const Vec2& worldMin, const Vec2& worldMax, 
    bool isMajor, float majorSpacingForAxisCheck) const 
{
    if (spacing <= 1e-6f) return;

    // Check rendering limits
    int estimatedLineCount;
    bool shouldRender;
    EnforceRenderingLimits(camera, viewport, spacing, worldMin, worldMax, estimatedLineCount, shouldRender);
    
    if (!shouldRender) {
        return; // Too many lines, skip rendering
    }

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
        
        const int max_reserve_per_axis = std::min(estimatedLineCount, 10000); // Respect the calculated limit
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

    // Check rendering limits
    int estimatedLineCount; // Used as an approximation for estimating dot count
    bool shouldRender;
    EnforceRenderingLimits(camera, viewport, spacing, worldMin, worldMax, estimatedLineCount, shouldRender);
    
    if (!shouldRender) {
        return; // Too many dots, skip rendering
    }

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
        
        // Use MAX_RENDERABLE_DOTS as the cap for dot reservation
        const int max_reserve_total_dots = m_settings->MAX_RENDERABLE_DOTS; 
        if (num_potential_dots_x > 0 && num_potential_dots_y > 0) {
            if (static_cast<long long>(num_potential_dots_x) * num_potential_dots_y < max_reserve_total_dots) {
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
                        } else {
                            // Reached limit, stop adding
                            break;
                        }
                    }
                }
            }
            if (dotsCount >= max_reserve_total_dots) {
                break; // Reached limit, stop adding
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
    // All spacing validity checks are already done in Render method
    if (actuallyDrawMinorLines) {
        DrawGridLines(bl_ctx, camera, viewport, minorSpacing, m_settings->m_minorLineColor, worldMin, worldMax, false, majorSpacing);
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
    // All spacing validity checks are already done in Render method
    if (actuallyDrawMinorDots) {
        DrawGridDots(bl_ctx, camera, viewport, minorSpacing, m_settings->m_minorLineColor, worldMin, worldMax, false, majorSpacing);
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

        // Draw background if needed
        if (m_settings->m_backgroundColor.a > 0.0f) {
            bl_ctx.setFillStyle(BLRgba32(
                static_cast<uint32_t>(m_settings->m_backgroundColor.r * 255.0f),
                static_cast<uint32_t>(m_settings->m_backgroundColor.g * 255.0f),
                static_cast<uint32_t>(m_settings->m_backgroundColor.b * 255.0f),
                static_cast<uint32_t>(m_settings->m_backgroundColor.a * 255.0f)
            ));
            bl_ctx.fillRect(BLRect(0, 0, viewport.GetWidth(), viewport.GetHeight()));
        }

        // Calculate effective grid spacings based on current zoom
        float effMajorSpacing_world, effMinorSpacing_world;
        int effectiveSubdivisionsToConsider;
        GetEffectiveSpacings(camera, effMajorSpacing_world, effMinorSpacing_world, effectiveSubdivisionsToConsider);

        // Get visible world bounds
        Vec2 worldMin, worldMax;
        GetVisibleWorldBounds(camera, viewport, worldMin, worldMax);
        
        const float currentZoom = camera.GetZoom();
        const float minPxStepSetting = std::max(1.0f, m_settings->m_minPixelStep);

        // --- Culling Decision based on minPixelStep (APPLIES TO BOTH DYNAMIC AND STATIC) ---
        bool actuallyDrawMajorElements = false;
        float majorScreenPx = effMajorSpacing_world * currentZoom;
        if (majorScreenPx >= minPxStepSetting) {
            actuallyDrawMajorElements = true;
        }

        bool actuallyDrawMinorElements = false;
        if (effectiveSubdivisionsToConsider > 1) { // Only consider minors if there are any
            float minorScreenPx = effMinorSpacing_world * currentZoom;
            // Check that minor spacing is meaningfully different from major,
            // and that minor spacing itself is positive.
            if (minorScreenPx >= minPxStepSetting && 
                std::abs(effMajorSpacing_world - effMinorSpacing_world) > 1e-6f && 
                effMinorSpacing_world > 1e-6f) {
                actuallyDrawMinorElements = true;
            }
        }

        // If major lines are hidden, it doesn't make sense to show minor lines either
        if (!actuallyDrawMajorElements) {
            actuallyDrawMinorElements = false;
        }
        
        // Clip to viewport bounds
        bl_ctx.clipToRect(BLRect(0, 0, viewport.GetWidth(), viewport.GetHeight()));

        // Draw grid elements
        if (m_settings->m_style == GridStyle::LINES) {
            DrawLinesStyle(bl_ctx, camera, viewport, effMajorSpacing_world, effMinorSpacing_world, worldMin, worldMax, actuallyDrawMajorElements, actuallyDrawMinorElements);
        } else if (m_settings->m_style == GridStyle::DOTS) {
            DrawDotsStyle(bl_ctx, camera, viewport, effMajorSpacing_world, effMinorSpacing_world, worldMin, worldMax, actuallyDrawMajorElements, actuallyDrawMinorElements);
        }

        // Draw axis lines (independent of grid culling)
        DrawAxis(bl_ctx, camera, viewport, worldMin, worldMax);
        
        // Create measurement readout info
        GridMeasurementInfo info;
        info.majorSpacing = effMajorSpacing_world;
        info.minorSpacing = effMinorSpacing_world;
        info.subdivisions = effectiveSubdivisionsToConsider;
        info.majorLinesVisible = actuallyDrawMajorElements;
        info.minorLinesVisible = actuallyDrawMinorElements;
        info.unitString = m_settings->UnitToString();
        
        // Draw measurement readout
        RenderMeasurementReadout(bl_ctx, viewport, info);

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
 
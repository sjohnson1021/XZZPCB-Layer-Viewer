#include "Grid.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <math.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <blend2d.h>  // Include for BLContext and BLRgba32 etc.

#include "GridSettings.hpp"

#include "view/Camera.hpp"
#include "view/GridSettings.hpp"
#include "view/Viewport.hpp"

Grid::Grid(std::shared_ptr<GridSettings> settings) : m_settings_(settings) {}

Grid::~Grid() = default;

void Grid::GetNiceUnitFactors(std::vector<float>& out_factors) const
{
    if (m_settings_->m_unit_system == GridUnitSystem::kMetric) {
        // Metric: Clean multipliers for scaling base spacing
        // Use standard 1-2-5 series: 0.1×, 0.2×, 0.5×, 1×, 2×, 5×, 10×, 20×, 50×, 100×
        out_factors = {0.1F, 0.2F, 0.5F, 1.0F, 2.0F, 5.0F, 10.0F, 20.0F, 50.0F, 100.0F};
    } else {
        // Imperial: Standard PCB design increments
        // Common fractions and decimal inches: 1/16, 1/8, 1/4, 1/2, 1, 2, 4, 6, 12
        out_factors = {0.0625F, 0.125F, 0.25F, 0.5F, 1.0F, 2.0F, 4.0F, 6.0F, 12.0F};
    }
}

void Grid::GetEffectiveSpacings(const Camera& camera, float& out_major_spacing_world, float& out_minor_spacing_world, int& out_effective_subdivisions) const
{
    // --- 0. Input Sanitization & Defaults ---
    float base_major_spacing_world = m_settings_->m_base_major_spacing;
    // Ensure positive base spacing, defaulting to appropriate unit-specific value if too small
    if (base_major_spacing_world <= 1e-6F) {
        base_major_spacing_world = (m_settings_->m_unit_system == GridUnitSystem::kMetric) ? 10.0F : 0.5F;
    }

    // Get appropriate subdivisions based on unit system if none specified
    int base_subdivisions = std::max(1, m_settings_->m_subdivisions);
    if (base_subdivisions <= 1) {
        base_subdivisions = (m_settings_->m_unit_system == GridUnitSystem::kMetric) ? 10 : 4;
    }

    float min_px_step = std::max(1.0F, m_settings_->m_min_pixel_step);
    float max_px_step = std::max(min_px_step * 1.5F, m_settings_->m_max_pixel_step);
    float zoom = std::max(1e-6F, camera.GetZoom());

    // Initialize outputs
    out_major_spacing_world = base_major_spacing_world;
    out_effective_subdivisions = base_subdivisions;

    // --- 1. Dynamic Spacing Mode ---
    if (m_settings_->m_is_dynamic) {
        // 1a. Determine Ideal Major Spacing based on zoom and pixel steps
        float const current_major_screen_px = out_major_spacing_world * zoom;

        if (current_major_screen_px < min_px_step || current_major_screen_px > max_px_step) {
            float best_fit_major_spacing = out_major_spacing_world;
            float smallest_diff = std::numeric_limits<float>::max();
            float target_screen_px = (min_px_step + max_px_step) / 2.0F;  // Aim for middle of the range

            // Get "nice" multiplier factors appropriate for the current unit system
            std::vector<float> nice_factors;
            GetNiceUnitFactors(nice_factors);

            // Try each multiplier applied to the user's base spacing
            // This ensures we always get clean multiples of the user's chosen base spacing
            for (float factor : nice_factors) {
                float candidate_spacing_world = base_major_spacing_world * factor;
                if (candidate_spacing_world < 1e-6F)
                    continue;  // Avoid too small spacing

                float candidate_screen_px = candidate_spacing_world * zoom;
                if (candidate_screen_px >= min_px_step && candidate_screen_px <= max_px_step) {
                    float diff = std::abs(candidate_screen_px - target_screen_px);
                    if (diff < smallest_diff) {
                        smallest_diff = diff;
                        best_fit_major_spacing = candidate_spacing_world;
                    }
                }
            }

            // If no "nice number" fit was found, use power-of-2 scaling as fallback
            if (smallest_diff == std::numeric_limits<float>::max()) {
                if (current_major_screen_px < min_px_step) {
                    float const kScale = min_px_step / current_major_screen_px;
                    best_fit_major_spacing = out_major_spacing_world * std::pow(2.0F, std::ceil(std::log2(kScale)));
                } else {  // currentMajorScreenPx > maxPxStep
                    float const kScale = max_px_step / current_major_screen_px;
                    if (kScale > 0) {
                        best_fit_major_spacing = out_major_spacing_world * std::pow(2.0F, std::floor(std::log2(kScale)));
                    }
                }
            }
            out_major_spacing_world = best_fit_major_spacing;
        }

        // Clamp to reasonable world unit bounds
        out_major_spacing_world = std::max(1e-7F, std::min(1e7F, out_major_spacing_world));

        // 1b. Determine Effective Subdivisions based on the new outMajorSpacing_world
        if (base_subdivisions > 1) {
            float const potential_minor_world_spacing = out_major_spacing_world / base_subdivisions;
            float const potential_minor_screen_px = potential_minor_world_spacing * zoom;

            if (potential_minor_screen_px < min_px_step) {
                // Try to find appropriate subdivision count based on unit system
                int new_subdivisions = base_subdivisions;

                if (m_settings_->m_unit_system == GridUnitSystem::kMetric) {
                    // For metric, prefer 10, 5, 2, 1
                    while (new_subdivisions > 1 && (out_major_spacing_world / new_subdivisions) * zoom < min_px_step) {
                        if (new_subdivisions == 10) {
                            new_subdivisions = 5;
                        } else if (new_subdivisions == 5) {
                            new_subdivisions = 2;
                        } else if (new_subdivisions == 2) {
                            new_subdivisions = 1;
                        } else {
                            new_subdivisions = 1;  // Fallback
                        }
                    }
                } else {
                    // For imperial, prefer 8, 4, 2, 1 (power of 2 divisions)
                    while (new_subdivisions > 1 && (out_major_spacing_world / new_subdivisions) * zoom < min_px_step) {
                        if (new_subdivisions == 16) {
                            new_subdivisions = 8;
                        } else if (new_subdivisions == 8) {
                            new_subdivisions = 4;
                        } else if (new_subdivisions == 4) {
                            new_subdivisions = 2;
                        } else if (new_subdivisions == 2) {
                            new_subdivisions = 1;
                        } else {
                            new_subdivisions = 1;  // Fallback
                        }
                    }
                }

                out_effective_subdivisions = std::max(1, new_subdivisions);
            } else {
                out_effective_subdivisions = base_subdivisions;
            }
        } else {
            out_effective_subdivisions = 1;
        }
    }
    // --- 2. Static Spacing Mode ---
    else {
        out_major_spacing_world = base_major_spacing_world;
        out_effective_subdivisions = base_subdivisions;
    }

    // --- 3. Final Minor Spacing Calculation ---
    out_minor_spacing_world = out_major_spacing_world / out_effective_subdivisions;
}

void Grid::GetVisibleWorldBounds(const Camera& camera, const Viewport& viewport, Vec2& out_min_world, Vec2& out_max_world)
{
    Vec2 screen_corners[4] = {{static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY())},
                              {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY())},
                              {static_cast<float>(viewport.GetX()), static_cast<float>(viewport.GetY() + viewport.GetHeight())},
                              {static_cast<float>(viewport.GetX() + viewport.GetWidth()), static_cast<float>(viewport.GetY() + viewport.GetHeight())}};
    Vec2 world_corners[4];
    for (int i = 0; i < 4; ++i) {
        world_corners[i] = viewport.ScreenToWorld(screen_corners[i], camera);
    }
    out_min_world = world_corners[0];
    out_max_world = world_corners[0];
    for (int i = 1; i < 4; ++i) {
        out_min_world.x_ax = std::min(out_min_world.x_ax, world_corners[i].x_ax);
        out_min_world.y_ax = std::min(out_min_world.y_ax, world_corners[i].y_ax);
        out_max_world.x_ax = std::max(out_max_world.x_ax, world_corners[i].x_ax);
        out_max_world.y_ax = std::max(out_max_world.y_ax, world_corners[i].y_ax);
    }
}

void Grid::EnforceRenderingLimits(
    const Camera& /*camera*/, const Viewport& /*viewport*/, float spacing, const Vec2& world_min, const Vec2& world_max, int& out_estimated_line_count, bool& out_should_render) const
{
    if (spacing <= 1e-6F) {
        out_estimated_line_count = 0;
        out_should_render = false;
        return;
    }

    // Calculate how many horizontal and vertical lines would be drawn
    auto i_start_x = static_cast<long long>(std::ceil(world_min.x_ax / spacing));
    auto i_end_x = static_cast<long long>(std::floor(world_max.x_ax / spacing));
    int const k_num_horizontal_lines = (i_end_x >= i_start_x) ? static_cast<int>(i_end_x - i_start_x + 1) : 0;

    auto i_start_y = static_cast<long long>(std::ceil(world_min.y_ax / spacing));
    auto i_end_y = static_cast<long long>(std::floor(world_max.y_ax / spacing));
    int const k_num_vertical_lines = (i_end_y >= i_start_y) ? static_cast<int>(i_end_y - i_start_y + 1) : 0;

    // Total number of lines
    out_estimated_line_count = k_num_horizontal_lines + k_num_vertical_lines;

    // For dots, the estimate would be numHorizontalLines * numVerticalLines
    // We'll use the same mechanism for both since dots also require two nested loops
    int const k_dot_estimate = k_num_horizontal_lines * k_num_vertical_lines;

    // Determine if we should render based on limits
    int max_renderable_items = 0;
    if (m_settings_->m_style == GridStyle::kLines) {
        max_renderable_items = GridSettings::kMaxRenderableLines;
        out_should_render = (out_estimated_line_count <= max_renderable_items);
    } else {  // DOTS
        max_renderable_items = GridSettings::kMaxRenderableDots;
        out_should_render = (k_dot_estimate <= max_renderable_items);
    }
}

Grid::GridMeasurementInfo Grid::GetMeasurementInfo(const Camera& camera, const Viewport& /*viewport*/) const
{
    GridMeasurementInfo info;

    // Get current effective spacings
    GetEffectiveSpacings(camera, info.major_spacing, info.minor_spacing, info.subdivisions);

    // Determine visibility based on screen pixel step
    float const current_zoom = camera.GetZoom();
    float min_px_step = std::max(1.0F, m_settings_->m_min_pixel_step);

    float const major_screen_px = info.major_spacing * current_zoom;
    info.major_lines_visible = (major_screen_px >= min_px_step);

    float const minor_screen_px = info.minor_spacing * current_zoom;
    info.minor_lines_visible =
        info.major_lines_visible && info.subdivisions > 1 && (minor_screen_px >= min_px_step) && (std::abs(info.major_spacing - info.minor_spacing) > 1e-6F) && (info.minor_spacing > 1e-6F);

    // Get unit string
    info.unit_string = m_settings_->UnitToString();

    return info;
}

void Grid::RenderMeasurementReadout(BLContext& bl_ctx, const Viewport& viewport, const GridMeasurementInfo& info) const
{
    if (!m_settings_->m_show_measurement_readout) {
        return;
    }

    // Ensure font is initialized
    InitializeFont();

    // Create the measurement text
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3);  // Display up to 3 decimal places

    // Convert world coordinates to proper display units based on current unit system
    bool is_metric = (m_settings_->m_unit_system == GridUnitSystem::kMetric);

    // Format major spacing
    if (info.major_lines_visible) {
        float display_major_spacing;
        if (is_metric) {
            display_major_spacing = GridSettings::WorldUnitsToMm(info.major_spacing);
        } else {
            display_major_spacing = GridSettings::WorldUnitsToInches(info.major_spacing);
        }
        ss << "Major: " << display_major_spacing << info.unit_string;
    } else {
        ss << "Major: Hidden";
    }

    // Format minor spacing
    ss << " | ";
    if (info.minor_lines_visible) {
        float display_minor_spacing;
        if (is_metric) {
            display_minor_spacing = GridSettings::WorldUnitsToMm(info.minor_spacing);
        } else {
            display_minor_spacing = GridSettings::WorldUnitsToInches(info.minor_spacing);
        }
        ss << "Minor: " << display_minor_spacing << info.unit_string;
    } else {
        ss << "Minor: Hidden";
    }

    std::string readout_text = ss.str();

    // Setup text position and size
    const int kPadding = 10;
    const auto kPntX = static_cast<float>(viewport.GetX() + kPadding);
    const auto kPntY = static_cast<float>(viewport.GetY() + viewport.GetHeight() - kPadding - 20);

    // Estimate text dimensions based on character count (rough approximation)
    // Average character width is about 8 pixels in a standard font at ~11px size
    float const kCharWidth = 8.0F;
    float text_width = static_cast<float>(readout_text.length()) * kCharWidth;
    float const kTextHeight = 16.0F;  // Typical text height with some padding

    // Draw background with semi-transparent black
    const float kBoxX = kPntX - 5.0F;
    const float kBoxY = kPntY - kTextHeight + 5.0F;
    const float kBoxWidth = text_width + 10.0F;
    const float kBoxHeight = kTextHeight + 10.0F;

    bl_ctx.setFillStyle(BLRgba32(0, 0, 0, 196));
    bl_ctx.fillRect(BLRect(kBoxX, kBoxY, kBoxWidth, kBoxHeight));

    if (!m_font_load_failed_) {
        // Draw text with solid white if font creation succeeded
        bl_ctx.setFillStyle(BLRgba32(255, 255, 255, 255));
        bl_ctx.fillUtf8Text(BLPoint(kPntX, kPntY), m_font_, readout_text.c_str(), readout_text.length());
    } else {
        // Fallback if font face creation failed - draw simple text indicator
        bl_ctx.setFillStyle(BLRgba32(255, 255, 255, 255));
        for (size_t i = 0; i < readout_text.length(); i++) {
            bl_ctx.fillRect(BLRect(kPntX + (i * kCharWidth), kPntY - (kTextHeight / 2), (readout_text[i] == ' ') ? kCharWidth / 4 : kCharWidth / 2, 1));
        }
    }
}

void Grid::DrawGridLines(BLContext& bl_ctx,
                         const Camera& camera,
                         const Viewport& viewport,
                         float spacing,
                         const BLRgba32& color,
                         const Vec2& world_min,
                         const Vec2& world_max,
                         bool is_major,
                         float major_spacing_for_axis_check) const

{
    if (spacing <= 1e-6F) {
        return;
    }

    // Check rendering limits
    int estimated_line_count = 0;
    bool should_render = false;
    EnforceRenderingLimits(camera, viewport, spacing, world_min, world_max, estimated_line_count, should_render);

    if (!should_render) {
        return;  // Too many lines, skip rendering
    }

    try {
        BLRgba32 line_color(static_cast<uint32_t>(color.r()), static_cast<uint32_t>(color.g()), static_cast<uint32_t>(color.b()), static_cast<uint32_t>(color.a()));

        BLPath lines_path;

        auto i_start_x = static_cast<long long>(std::ceil(world_min.x_ax / spacing));
        auto i_end_x = static_cast<long long>(std::floor(world_max.x_ax / spacing));
        int const kReserveHorizLines = (i_end_x >= i_start_x) ? static_cast<int>(i_end_x - i_start_x + 1) : 0;

        auto i_start_y = static_cast<long long>(std::ceil(world_min.y_ax / spacing));
        auto i_end_y = static_cast<long long>(std::floor(world_max.y_ax / spacing));
        int const kReserveVertLines = (i_end_y >= i_start_y) ? static_cast<int>(i_end_y - i_start_y + 1) : 0;

        const int kMaxReservePerAxis = std::min(estimated_line_count, 10000);  // Respect the calculated limit
        lines_path.reserve((std::min(kReserveHorizLines, kMaxReservePerAxis) + std::min(kReserveVertLines, kMaxReservePerAxis)) * 2);

        std::vector<Vec2> vertical_start_points;
        std::vector<Vec2> vertical_end_points;
        vertical_start_points.reserve(std::min(kReserveHorizLines, kMaxReservePerAxis));
        vertical_end_points.reserve(std::min(kReserveHorizLines, kMaxReservePerAxis));

        std::vector<Vec2> horizontal_start_points;
        std::vector<Vec2> horizontal_end_points;
        horizontal_start_points.reserve(std::min(kReserveVertLines, kMaxReservePerAxis));
        horizontal_end_points.reserve(std::min(kReserveVertLines, kMaxReservePerAxis));

        for (long long i = i_start_x; i <= i_end_x; ++i) {
            float const kX = static_cast<float>(i) * spacing;
            if (is_major && m_settings_->m_show_axis_lines && std::abs(kX) < major_spacing_for_axis_check * 0.1F) {
                continue;
            }
            Vec2 screen_p1 = viewport.WorldToScreen({kX, world_min.y_ax}, camera);
            Vec2 screen_p2 = viewport.WorldToScreen({kX, world_max.y_ax}, camera);
            if (std::isfinite(screen_p1.x_ax) && std::isfinite(screen_p1.y_ax) && std::isfinite(screen_p2.x_ax) && std::isfinite(screen_p2.y_ax)) {
                vertical_start_points.push_back(screen_p1);
                vertical_end_points.push_back(screen_p2);
            }
        }

        for (long long i = i_start_y; i <= i_end_y; ++i) {
            float const kY = static_cast<float>(i) * spacing;
            if (is_major && m_settings_->m_show_axis_lines && std::abs(kY) < major_spacing_for_axis_check * 0.1F) {
                continue;
            }
            Vec2 screen_p1 = viewport.WorldToScreen({world_min.x_ax, kY}, camera);
            Vec2 screen_p2 = viewport.WorldToScreen({world_max.x_ax, kY}, camera);
            if (std::isfinite(screen_p1.x_ax) && std::isfinite(screen_p1.y_ax) && std::isfinite(screen_p2.x_ax) && std::isfinite(screen_p2.y_ax)) {
                horizontal_start_points.push_back(screen_p1);
                horizontal_end_points.push_back(screen_p2);
            }
        }

        for (size_t i = 0; i < vertical_start_points.size(); i++) {
            lines_path.moveTo(vertical_start_points[i].x_ax, vertical_start_points[i].y_ax);
            lines_path.lineTo(vertical_end_points[i].x_ax, vertical_end_points[i].y_ax);
        }

        for (size_t i = 0; i < horizontal_start_points.size(); i++) {
            lines_path.moveTo(horizontal_start_points[i].x_ax, horizontal_start_points[i].y_ax);
            lines_path.lineTo(horizontal_end_points[i].x_ax, horizontal_end_points[i].y_ax);
        }

        if (!lines_path.empty()) {
            bl_ctx.setStrokeStyle(line_color);
            bl_ctx.setStrokeWidth(m_settings_->m_line_thickness);
            BLCompOp saved_comp_op = bl_ctx.compOp();
            if (line_color.a() == 255) {
                bl_ctx.setCompOp(BL_COMP_OP_SRC_OVER);
            }
            bl_ctx.strokePath(lines_path);
            if (line_color.a() == 255) {
                bl_ctx.setCompOp(saved_comp_op);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Grid::DrawGridLines Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Grid::DrawGridLines: Unknown exception" << std::endl;
    }
}

void Grid::DrawGridDots(BLContext& bl_ctx,
                        const Camera& camera,
                        const Viewport& viewport,
                        float spacing,
                        const BLRgba32& color,
                        const Vec2& world_min,
                        const Vec2& world_max,
                        bool is_major,
                        float major_spacing_for_axis_check) const

{
    if (spacing <= 1e-6F) {
        return;
    }

    // Check rendering limits
    int estimated_line_count = 0;  // Used as an approximation for estimating dot count
    bool should_render = false;
    EnforceRenderingLimits(camera, viewport, spacing, world_min, world_max, estimated_line_count, should_render);

    if (!should_render) {
        return;  // Too many dots, skip rendering
    }

    try {
        BLRgba32 dot_color(static_cast<uint32_t>(color.r()), static_cast<uint32_t>(color.g()), static_cast<uint32_t>(color.b()), static_cast<uint32_t>(color.a()));

        float dot_radius = m_settings_->m_dot_radius;
        BLPath dots_path;
        int dots_count = 0;

        auto i_start_x = static_cast<long long>(std::ceil(world_min.x_ax / spacing));
        auto i_end_x = static_cast<long long>(std::floor(world_max.x_ax / spacing));
        auto i_start_y = static_cast<long long>(std::ceil(world_min.y_ax / spacing));
        auto i_end_y = static_cast<long long>(std::floor(world_max.y_ax / spacing));

        int const num_potential_dots_x = (i_end_x >= i_start_x) ? static_cast<int>(i_end_x - i_start_x + 1) : 0;
        int const num_potential_dots_y = (i_end_y >= i_start_y) ? static_cast<int>(i_end_y - i_start_y + 1) : 0;

        // Use MAX_RENDERABLE_DOTS as the cap for dot reservation
        constexpr int kMaxReserveTotalDots = GridSettings::kMaxRenderableDots;
        if (num_potential_dots_x > 0 && num_potential_dots_y > 0) {
            if (static_cast<long long>(num_potential_dots_x) * num_potential_dots_y < kMaxReserveTotalDots) {
                dots_path.reserve(num_potential_dots_x * num_potential_dots_y);
            } else {
                dots_path.reserve(kMaxReserveTotalDots);  // Cap reservation
            }
        }

        for (long long i_x = i_start_x; i_x <= i_end_x; ++i_x) {
            float x = static_cast<float>(i_x) * spacing;
            for (long long i_y = i_start_y; i_y <= i_end_y; ++i_y) {
                float y = static_cast<float>(i_y) * spacing;

                if (is_major && m_settings_->m_show_axis_lines && (std::abs(x) < major_spacing_for_axis_check * 0.1f || std::abs(y) < major_spacing_for_axis_check * 0.1f)) {
                    continue;
                }

                Vec2 screen_p = viewport.WorldToScreen({x, y}, camera);

                if (std::isfinite(screen_p.x_ax) && std::isfinite(screen_p.y_ax)) {
                    if (screen_p.x_ax >= -dot_radius && screen_p.x_ax <= viewport.GetWidth() + dot_radius && screen_p.y_ax >= -dot_radius && screen_p.y_ax <= viewport.GetHeight() + dot_radius) {
                        if (dots_count < kMaxReserveTotalDots) {
                            dots_path.addCircle(BLCircle(screen_p.x_ax, screen_p.y_ax, dot_radius));
                            dots_count++;
                        } else {
                            // Reached limit, stop adding
                            break;
                        }
                    }
                }
            }
            if (dots_count >= kMaxReserveTotalDots) {
                break;  // Reached limit, stop adding
            }
        }

        if (dots_count > 0) {
            bl_ctx.setFillStyle(dot_color);
            bl_ctx.fillPath(dots_path);
        }
    } catch (const std::exception& e) {
        std::cerr << "Grid::DrawGridDots Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Grid::DrawGridDots: Unknown exception" << std::endl;
    }
}

void Grid::DrawLinesStyle(BLContext& bl_ctx,
                          const Camera& camera,
                          const Viewport& viewport,
                          float major_spacing,
                          float minor_spacing,
                          const Vec2& world_min,
                          const Vec2& world_max,
                          bool actually_draw_major_lines,
                          bool actually_draw_minor_lines) const
{
    // All spacing validity checks are already done in Render method
    if (actually_draw_minor_lines) {
        DrawGridLines(bl_ctx, camera, viewport, minor_spacing, m_settings_->m_minor_line_color, world_min, world_max, false, major_spacing);
    }
    if (actually_draw_major_lines) {
        DrawGridLines(bl_ctx, camera, viewport, major_spacing, m_settings_->m_major_line_color, world_min, world_max, true, minor_spacing);
    }
}

void Grid::DrawDotsStyle(BLContext& bl_ctx,
                         const Camera& camera,
                         const Viewport& viewport,
                         float major_spacing,
                         float minor_spacing,
                         const Vec2& world_min,
                         const Vec2& world_max,
                         bool actually_draw_major_dots,
                         bool actually_draw_minor_dots) const
{
    // All spacing validity checks are already done in Render method
    if (actually_draw_minor_dots) {
        DrawGridDots(bl_ctx, camera, viewport, minor_spacing, m_settings_->m_minor_line_color, world_min, world_max, false, major_spacing);
    }
    if (actually_draw_major_dots) {
        DrawGridDots(bl_ctx, camera, viewport, major_spacing, m_settings_->m_major_line_color, world_min, world_max, true, major_spacing);
    }
}

void Grid::DrawAxis(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport, const Vec2& world_min, const Vec2& world_max) const
{
    if (!m_settings_->m_show_axis_lines) {
        return;
    }

    try {
        bl_ctx.setStrokeWidth(m_settings_->m_axis_line_thickness);

        // X-Axis (y=0)
        if (0.0F >= world_min.y_ax && 0.0F <= world_max.y_ax) {
            BLRgba32 x_axis_color(static_cast<uint32_t>(m_settings_->m_x_axis_color.r()),
                                  static_cast<uint32_t>(m_settings_->m_x_axis_color.g()),
                                  static_cast<uint32_t>(m_settings_->m_x_axis_color.b()),
                                  static_cast<uint32_t>(m_settings_->m_x_axis_color.a()));
            bl_ctx.setStrokeStyle(x_axis_color);

            Vec2 screen_start = viewport.WorldToScreen({world_min.x_ax, 0.0F}, camera);
            Vec2 screen_end = viewport.WorldToScreen({world_max.x_ax, 0.0F}, camera);

            if (std::isfinite(screen_start.x_ax) && std::isfinite(screen_start.y_ax) && std::isfinite(screen_end.x_ax) && std::isfinite(screen_end.y_ax)) {
                bl_ctx.strokeLine(screen_start.x_ax, screen_start.y_ax, screen_end.x_ax, screen_end.y_ax);
            }
        }

        // Y-Axis (x=0)
        if (0.0F >= world_min.x_ax && 0.0F <= world_max.x_ax) {
            BLRgba32 y_axis_color(static_cast<uint32_t>(m_settings_->m_y_axis_color.r()),
                                  static_cast<uint32_t>(m_settings_->m_y_axis_color.g()),
                                  static_cast<uint32_t>(m_settings_->m_y_axis_color.b()),
                                  static_cast<uint32_t>(m_settings_->m_y_axis_color.a()));
            bl_ctx.setStrokeStyle(y_axis_color);

            Vec2 screen_start = viewport.WorldToScreen({0.0F, world_min.y_ax}, camera);
            Vec2 screen_end = viewport.WorldToScreen({0.0F, world_max.y_ax}, camera);

            if (std::isfinite(screen_start.x_ax) && std::isfinite(screen_start.y_ax) && std::isfinite(screen_end.x_ax) && std::isfinite(screen_end.y_ax)) {
                bl_ctx.strokeLine(screen_start.x_ax, screen_start.y_ax, screen_end.x_ax, screen_end.y_ax);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Grid::DrawAxis Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Grid::DrawAxis: Unknown exception" << std::endl;
    }
}

void Grid::Render(BLContext& bl_ctx, const Camera& camera, const Viewport& viewport) const
{
    if (!m_settings_ || !m_settings_->m_visible) {
        return;
    }

    try {
        bl_ctx.save();

        // Draw background if needed
        if (m_settings_->m_background_color.a() > 0.0F) {
            bl_ctx.setFillStyle(BLRgba32(static_cast<uint32_t>(m_settings_->m_background_color.r()),
                                         static_cast<uint32_t>(m_settings_->m_background_color.g()),
                                         static_cast<uint32_t>(m_settings_->m_background_color.b()),
                                         static_cast<uint32_t>(m_settings_->m_background_color.a())));
            bl_ctx.fillRect(BLRect(0, 0, viewport.GetWidth(), viewport.GetHeight()));
        }

        // Calculate effective grid spacings based on current zoom
        float eff_major_spacing_world = 0.0F;
        float eff_minor_spacing_world = 0.0F;
        int effective_subdivisions_to_consider = 0;
        GetEffectiveSpacings(camera, eff_major_spacing_world, eff_minor_spacing_world, effective_subdivisions_to_consider);

        // Get visible world bounds
        Vec2 world_min, world_max;
        GetVisibleWorldBounds(camera, viewport, world_min, world_max);

        const float kCurrentZoom = camera.GetZoom();
        const float kMinPxStepSetting = std::max(1.0F, m_settings_->m_min_pixel_step);

        // --- Culling Decision based on minPixelStep (APPLIES TO BOTH DYNAMIC AND STATIC) ---
        bool actually_draw_major_elements = false;
        float const kMajorScreenPx = eff_major_spacing_world * kCurrentZoom;
        if (kMajorScreenPx >= kMinPxStepSetting) {
            actually_draw_major_elements = true;
        }

        bool actually_draw_minor_elements = false;
        if (effective_subdivisions_to_consider > 1) {  // Only consider minors if there are any
            float const kMinorScreenPx = eff_minor_spacing_world * kCurrentZoom;
            // Check that minor spacing is meaningfully different from major,
            // and that minor spacing itself is positive.
            if (kMinorScreenPx >= kMinPxStepSetting && std::abs(eff_major_spacing_world - eff_minor_spacing_world) > 1e-6F && eff_minor_spacing_world > 1e-6F) {
                actually_draw_minor_elements = true;
            }
        }

        // If major lines are hidden, it doesn't make sense to show minor lines either
        if (!actually_draw_major_elements) {
            actually_draw_minor_elements = false;
        }

        // Clip to viewport bounds
        bl_ctx.clipToRect(BLRect(0, 0, viewport.GetWidth(), viewport.GetHeight()));

        // Draw grid elements
        if (m_settings_->m_style == GridStyle::kLines) {
            DrawLinesStyle(bl_ctx, camera, viewport, eff_major_spacing_world, eff_minor_spacing_world, world_min, world_max, actually_draw_major_elements, actually_draw_minor_elements);
        } else if (m_settings_->m_style == GridStyle::kDots) {
            DrawDotsStyle(bl_ctx, camera, viewport, eff_major_spacing_world, eff_minor_spacing_world, world_min, world_max, actually_draw_major_elements, actually_draw_minor_elements);
        }

        // Draw axis lines (independent of grid culling)
        DrawAxis(bl_ctx, camera, viewport, world_min, world_max);

        // Create measurement readout info
        GridMeasurementInfo info;
        info.major_spacing = eff_major_spacing_world;
        info.minor_spacing = eff_minor_spacing_world;
        info.subdivisions = effective_subdivisions_to_consider;
        info.major_lines_visible = actually_draw_major_elements;
        info.minor_lines_visible = actually_draw_minor_elements;
        info.unit_string = m_settings_->UnitToString();

        // Draw measurement readout
        RenderMeasurementReadout(bl_ctx, viewport, info);

        bl_ctx.restore();
    } catch (const std::exception& e) {
        std::cerr << "Grid::Render Exception: " << e.what() << std::endl;
        try {
            bl_ctx.restore();
        } catch (...) {
        }
    } catch (...) {
        std::cerr << "Grid::Render: Unknown exception" << std::endl;
        try {
            bl_ctx.restore();
        } catch (...) {
        }
    }
}

// InitializeFont method implementation
void Grid::InitializeFont() const
{
    if (m_font_initialized_) {
        return;
    }

    m_font_initialized_ = true;   // Attempt initialization
    m_font_load_failed_ = false;  // Assume success initially

    // Use the project's font file
    BLResult face_result = m_font_face_.createFromFile("assets/fonts/Nippo-Light.otf");
    if (face_result != BL_SUCCESS) {
        // Try with full path if relative path fails
        // TODO: Consider making this path more robust or configurable
        face_result = m_font_face_.createFromFile("/home/seanj/Documents/Code/XZZPCB-Layer-Viewer/assets/fonts/Nippo-Light.otf");
        if (face_result != BL_SUCCESS) {
            m_font_load_failed_ = true;
            std::cerr << "Failed to load font file for grid measurement readout. Error code: " << face_result << std::endl;
            return;
        }
    }

    BLResult font_result = m_font_.createFromFace(m_font_face_, 11.0F);
    if (font_result != BL_SUCCESS) {
        m_font_load_failed_ = true;
        std::cerr << "Failed to create font from face for grid measurement readout. Error code: " << font_result << std::endl;
    }
}

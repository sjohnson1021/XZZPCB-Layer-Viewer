#include "OrientationProcessor.hpp"
#include "pcb/Board.hpp" // Required for Board class definition
#include "pcb/elements/Component.hpp"
#include "pcb/elements/Pin.hpp"
#include <map>       // For std::map in calculateComponentRotation
#include <algorithm> // For std::min, std::max, std::sort, std::find
#include <cmath>     // For std::sqrt, std::atan2, std::abs, std::cos, std::sin, M_PI
#include <limits>    // For std::numeric_limits

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// For debug printing, if needed
// #include <cstdio>

namespace PCBProcessing
{

    struct LocalLineSegment
    {
        BoardPoint2D start;
        BoardPoint2D end;
        double angle_rad_local; // Angle in the component\'s local, axis-aligned frame
        double length;

        LocalLineSegment(BoardPoint2D s, BoardPoint2D e) : start(s), end(e)
        {
            double dx = end.x - start.x;
            double dy = end.y - start.y;
            length = std::sqrt(dx * dx + dy * dy);
            if (length > 1e-9)
            { // Avoid division by zero or atan2(0,0) issues
                angle_rad_local = std::atan2(dy, dx);
            }
            else
            {
                angle_rad_local = 0.0;
            }
        }
    };

    // Helper function to calculate the square of the distance from a point to a line segment
    // Adapted from C++ code for shortest distance between point and line segment
    static double distSqPointToSegment(BoardPoint2D p, BoardPoint2D seg_a, BoardPoint2D seg_b)
    {
        double l2 = (seg_a.x - seg_b.x) * (seg_a.x - seg_b.x) + (seg_a.y - seg_b.y) * (seg_a.y - seg_b.y);
        if (l2 == 0.0)
            return (p.x - seg_a.x) * (p.x - seg_a.x) + (p.y - seg_a.y) * (p.y - seg_a.y); // seg_a == seg_b
        double t = ((p.x - seg_a.x) * (seg_b.x - seg_a.x) + (p.y - seg_a.y) * (seg_b.y - seg_a.y)) / l2;
        t = std::max(0.0, std::min(1.0, t)); // Clamp t to the range [0, 1]
        BoardPoint2D projection = {seg_a.x + t * (seg_b.x - seg_a.x),
                                   seg_a.y + t * (seg_b.y - seg_a.y)};
        return (p.x - projection.x) * (p.x - projection.x) + (p.y - projection.y) * (p.y - projection.y);
    }

    // Helper to convert radians to degrees, normalized to [0, 360)
    static double radToDeg(double rad)
    {
        double deg = rad * 180.0 / M_PI;
        while (deg < 0.0)
            deg += 360.0;
        while (deg >= 360.0)
            deg -= 360.0;
        return deg;
    }

    void OrientationProcessor::processBoard(Board &board)
    {
        for (Component &component : board.components)
        {
            // Initialize pin dimensions first
            for (Pin &pin : component.pins)
            {
                calculateInitialPinDimensions(pin);
            }

            // Skip if no pins or already processed (add a flag to Component if needed)
            if (component.pins.empty())
            {
                continue;
            }

            // Calculate component body rotation
            double body_rotation_rad = calculateComponentRotation(component);
            component.rotation = body_rotation_rad; // Store it on the component

            firstPass_AnalyzeComponent(component, body_rotation_rad);
            // The original logic iterates PinRenderInfo; we iterate Component.pins directly
            // and modify Pin.orientation and use Pin.initialWidth/Height.
            secondPass_CheckOverlapsAndBoundaries(component, body_rotation_rad);
            thirdPass_FinalBoundaryCheck(component, body_rotation_rad);
        }
    }

    // Helper function to rotate a point around the origin (0,0)
    static inline BoardPoint2D rotatePoint(double x, double y, double angle_rad)
    {
        double cos_a = std::cos(angle_rad);
        double sin_a = std::sin(angle_rad);
        return {x * cos_a - y * sin_a, x * sin_a + y * cos_a};
    }

    void OrientationProcessor::calculateInitialPinDimensions(Pin &pin)
    {
        // Initialize pin.initialWidth and pin.initialHeight based on pin.pad_shape
        std::visit([&](const auto &shape)
                   {
        using T = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<T, CirclePad>) {
            pin.initial_width = shape.radius * 2.0;
            pin.initial_height = shape.radius * 2.0;
        } else if constexpr (std::is_same_v<T, RectanglePad>) {
            pin.initial_width = shape.width;
            pin.initial_height = shape.height;
        // } else if constexpr (std::is_same_v<T, SquarePad>) { // Assuming SquarePad is distinct or handled by RectanglePad
        //     pin.initial_width = shape.side_length;
        //     pin.initial_height = shape.side_length;
        } else if constexpr (std::is_same_v<T, CapsulePad>) {
            pin.initial_width = shape.width;   // Total width of capsule
            pin.initial_height = shape.height; // Diameter / height part
        } }, pin.pad_shape);
        pin.short_side = std::min(pin.initial_width, pin.initial_height);
        pin.long_side = std::max(pin.initial_width, pin.initial_height);
        // Ensure natural orientation starts with width > height if applicable (e.g. for rectangles)
        // The rendering logic might swap them based on PinOrientation, this sets the baseline.
        // if (pin.initial_width < pin.initial_height) {
        //     std::swap(pin.initial_width, pin.initial_height);
        // }
    }

    void OrientationProcessor::firstPass_AnalyzeComponent(Component &component, double component_rotation_radians)
    {
        // component_rotation_radians is the angle to rotate the component *back* to be axis-aligned.
        // So, we rotate pin coordinates by -component_rotation_radians.
        double local_rotation_rad = -component_rotation_radians;

        // Create local versions of component graphical segments (for bounding box calculation)
        std::vector<LocalLineSegment> local_graphical_segments;
        if (!component.graphical_elements.empty())
        {
            for (const auto &seg : component.graphical_elements)
            {
                BoardPoint2D local_start = rotatePoint(seg.start.x, seg.start.y, local_rotation_rad);
                BoardPoint2D local_end = rotatePoint(seg.end.x, seg.end.y, local_rotation_rad);
                local_graphical_segments.emplace_back(local_start, local_end);
            }
        }
        else if (component.width > 1e-6 && component.height > 1e-6)
        {
            BoardPoint2D p1 = {-component.width / 2.0, -component.height / 2.0};
            BoardPoint2D p2 = {component.width / 2.0, -component.height / 2.0};
            BoardPoint2D p3 = {component.width / 2.0, component.height / 2.0};
            BoardPoint2D p4 = {-component.width / 2.0, component.height / 2.0};
            local_graphical_segments.emplace_back(p1, p2);
            local_graphical_segments.emplace_back(p2, p3);
            local_graphical_segments.emplace_back(p3, p4);
            local_graphical_segments.emplace_back(p4, p1);
        }

        component.is_single_pin = (component.pins.size() == 1);
        if (component.is_single_pin)
        {
            if (!component.pins.empty())
            {
                Pin &pin = component.pins[0];
                pin.orientation = (pin.initial_height > pin.initial_width) ? PinOrientation::Vertical : PinOrientation::Horizontal;
            }
            return;
        }

        // Store local pin positions
        std::vector<BoardPoint2D> local_pin_positions(component.pins.size());
        for (size_t i = 0; i < component.pins.size(); ++i)
        {
            local_pin_positions[i] = rotatePoint(component.pins[i].x_coord, component.pins[i].y_coord, local_rotation_rad);
        }

        // Calculate component local boundaries (comp_b notation for clarity)
        double comp_b_min_x = std::numeric_limits<double>::max();
        double comp_b_max_x = std::numeric_limits<double>::lowest();
        double comp_b_min_y = std::numeric_limits<double>::max();
        double comp_b_max_y = std::numeric_limits<double>::lowest();

        if (!local_graphical_segments.empty())
        {
            for (const auto &seg : local_graphical_segments)
            {
                comp_b_min_x = std::min({comp_b_min_x, seg.start.x, seg.end.x});
                comp_b_max_x = std::max({comp_b_max_x, seg.start.x, seg.end.x});
                comp_b_min_y = std::min({comp_b_min_y, seg.start.y, seg.end.y});
                comp_b_max_y = std::max({comp_b_max_y, seg.start.y, seg.end.y});
            }
        }
        else if (component.width > 1e-6 && component.height > 1e-6)
        { // From component dimensions if no graphics
            comp_b_min_x = -component.width / 2.0;
            comp_b_max_x = component.width / 2.0;
            comp_b_min_y = -component.height / 2.0;
            comp_b_max_y = component.height / 2.0;
        }
        else if (!component.pins.empty())
        { // Fallback to pin bounding box
            for (const auto &pos : local_pin_positions)
            {
                comp_b_min_x = std::min(comp_b_min_x, pos.x);
                comp_b_max_x = std::max(comp_b_max_x, pos.x);
                comp_b_min_y = std::min(comp_b_min_y, pos.y);
                comp_b_max_y = std::max(comp_b_max_y, pos.y);
            }
        }
        else
        { // No information to determine boundaries
            return;
        }

        // Update component's pin bounding box (used elsewhere or for reference)
        component.pin_bbox_min_x = comp_b_min_x;
        component.pin_bbox_max_x = comp_b_max_x;
        component.pin_bbox_min_y = comp_b_min_y;
        component.pin_bbox_max_y = comp_b_max_y;

        // Determine characteristic (wide/tall) based on actual component dimensions if available
        if (component.width > 1e-6 && component.height > 1e-6)
        {
            component.is_wide_component = component.width > component.height * 1.2;
            component.is_tall_component = component.height > component.width * 1.2;
        }
        else
        { // Fallback to pin bbox aspect if component dimensions unknown
            double pin_bbox_width = comp_b_max_x - comp_b_min_x;
            double pin_bbox_height = comp_b_max_y - comp_b_min_y;
            if (pin_bbox_width > 1e-6 && pin_bbox_height > 1e-6)
            {
                component.is_wide_component = pin_bbox_width > pin_bbox_height * 1.2;
                component.is_tall_component = pin_bbox_height > pin_bbox_width * 1.2;
            }
            else
            {
                component.is_wide_component = false;
                component.is_tall_component = false;
            }
        }

        double short_side_for_tol;
        if (component.width > 1e-6 && component.height > 1e-6)
        {
            short_side_for_tol = std::min(component.width, component.height);
        }
        else if ((comp_b_max_x - comp_b_min_x) > 1e-6 && (comp_b_max_y - comp_b_min_y) > 1e-6)
        {
            short_side_for_tol = std::min((comp_b_max_x - comp_b_min_x), (comp_b_max_y - comp_b_min_y));
        }
        else
        {
            short_side_for_tol = 1.0; // Default fallback
        }

        // --- New Edge Detection Logic ---
        std::map<size_t, LocalEdge> pin_to_local_edge_map;
        for (size_t i = 0; i < component.pins.size(); ++i)
            pin_to_local_edge_map[i] = LocalEdge::INTERIOR;

        if (component.pins.size() > 1)
        {                                                                              // Edge detection only relevant for multi-pin components
            const double discretization_resolution = short_side_for_tol * 0.05 + 1e-9; // Add epsilon to avoid division by zero if short_side_for_tol is tiny
            const unsigned int min_pins_for_line_group = 2;                            // Minimum pins to be considered a "line"
            // Max gap: if short_side_for_tol is small, use a multiple of it. If large, cap it.
            // Example: If short_side_for_tol = 0.1mm, max_gap = 0.5mm. If short_side_for_tol=10mm, max_gap=5mm.
            const double max_gap_factor = 2.0; // Pins further apart than this factor times their typical size are not in the same line segment.
                                               // Using average short side of pins as a reference for gap.
            double avg_pin_short_side = 0.0;
            if (!component.pins.empty())
            {
                for (const auto &p : component.pins)
                    avg_pin_short_side += p.short_side;
                avg_pin_short_side /= component.pins.size();
            }
            if (avg_pin_short_side < 1e-6)
                avg_pin_short_side = short_side_for_tol * 0.1; // Fallback if pins have no size.
            const double max_gap_in_line = std::max(avg_pin_short_side * max_gap_factor, discretization_resolution * 5.0);

            std::map<int, std::vector<size_t>> x_coord_map;
            std::map<int, std::vector<size_t>> y_coord_map;

            for (size_t i = 0; i < component.pins.size(); ++i)
            {
                int disc_x = static_cast<int>(std::round(local_pin_positions[i].x / discretization_resolution));
                int disc_y = static_cast<int>(std::round(local_pin_positions[i].y / discretization_resolution));
                x_coord_map[disc_x].push_back(i);
                y_coord_map[disc_y].push_back(i);
            }

            std::vector<std::vector<size_t>> vertical_pin_lines;
            for (auto const &[disc_x, pin_indices_at_x] : x_coord_map)
            {
                if (pin_indices_at_x.size() < min_pins_for_line_group)
                    continue;
                std::vector<size_t> sorted_indices = pin_indices_at_x;
                std::sort(sorted_indices.begin(), sorted_indices.end(), [&](size_t a, size_t b)
                          { return local_pin_positions[a].y < local_pin_positions[b].y; });

                std::vector<size_t> current_line;
                if (!sorted_indices.empty())
                {
                    current_line.push_back(sorted_indices[0]);
                    for (size_t i = 1; i < sorted_indices.size(); ++i)
                    {
                        if (local_pin_positions[sorted_indices[i]].y - local_pin_positions[sorted_indices[i - 1]].y > max_gap_in_line)
                        {
                            if (current_line.size() >= min_pins_for_line_group)
                                vertical_pin_lines.push_back(current_line);
                            current_line.clear();
                        }
                        current_line.push_back(sorted_indices[i]);
                    }
                    if (current_line.size() >= min_pins_for_line_group)
                        vertical_pin_lines.push_back(current_line);
                }
            }

            std::vector<std::vector<size_t>> horizontal_pin_lines;
            for (auto const &[disc_y, pin_indices_at_y] : y_coord_map)
            {
                if (pin_indices_at_y.size() < min_pins_for_line_group)
                    continue;
                std::vector<size_t> sorted_indices = pin_indices_at_y;
                std::sort(sorted_indices.begin(), sorted_indices.end(), [&](size_t a, size_t b)
                          { return local_pin_positions[a].x < local_pin_positions[b].x; });

                std::vector<size_t> current_line;
                if (!sorted_indices.empty())
                {
                    current_line.push_back(sorted_indices[0]);
                    for (size_t i = 1; i < sorted_indices.size(); ++i)
                    {
                        if (local_pin_positions[sorted_indices[i]].x - local_pin_positions[sorted_indices[i - 1]].x > max_gap_in_line)
                        {
                            if (current_line.size() >= min_pins_for_line_group)
                                horizontal_pin_lines.push_back(current_line);
                            current_line.clear();
                        }
                        current_line.push_back(sorted_indices[i]);
                    }
                    if (current_line.size() >= min_pins_for_line_group)
                        horizontal_pin_lines.push_back(current_line);
                }
            }

            const double edge_zone_thickness = short_side_for_tol * 0.25; // Pins within 25% of short side from boundary are edge pins

            for (const auto &line : vertical_pin_lines)
            {
                double avg_x = 0;
                for (size_t pin_idx : line)
                    avg_x += local_pin_positions[pin_idx].x;
                avg_x /= line.size();
                if (avg_x < comp_b_min_x + edge_zone_thickness)
                {
                    for (size_t pin_idx : line)
                        pin_to_local_edge_map[pin_idx] = LocalEdge::LEFT;
                }
                else if (avg_x > comp_b_max_x - edge_zone_thickness)
                {
                    for (size_t pin_idx : line)
                        pin_to_local_edge_map[pin_idx] = LocalEdge::RIGHT;
                }
            }
            for (const auto &line : horizontal_pin_lines)
            {
                double avg_y = 0;
                for (size_t pin_idx : line)
                    avg_y += local_pin_positions[pin_idx].y;
                avg_y /= line.size();
                if (avg_y < comp_b_min_y + edge_zone_thickness)
                { // Pins on bottom edge
                    for (size_t pin_idx : line)
                    {
                        // If already on LEFT/RIGHT, it's a corner. Prioritize TOP/BOTTOM for now if also on horizontal edge.
                        pin_to_local_edge_map[pin_idx] = LocalEdge::BOTTOM;
                    }
                }
                else if (avg_y > comp_b_max_y - edge_zone_thickness)
                { // Pins on top edge
                    for (size_t pin_idx : line)
                    {
                        pin_to_local_edge_map[pin_idx] = LocalEdge::TOP;
                    }
                }
            }
        }
        // --- End New Edge Detection Logic ---

        component.left_edge_pin_indices.clear();
        component.right_edge_pin_indices.clear();
        component.top_edge_pin_indices.clear();
        component.bottom_edge_pin_indices.clear();

        for (size_t i = 0; i < component.pins.size(); ++i)
        {
            component.pins[i].local_edge = pin_to_local_edge_map[i]; // Store on pin itself
            if (pin_to_local_edge_map[i] == LocalEdge::LEFT)
                component.left_edge_pin_indices.push_back(i);
            else if (pin_to_local_edge_map[i] == LocalEdge::RIGHT)
                component.right_edge_pin_indices.push_back(i);
            else if (pin_to_local_edge_map[i] == LocalEdge::TOP)
                component.top_edge_pin_indices.push_back(i);
            else if (pin_to_local_edge_map[i] == LocalEdge::BOTTOM)
                component.bottom_edge_pin_indices.push_back(i);
        }

        component.is_qfp = (component.left_edge_pin_indices.size() >= 2 && // Relaxed QFP detection slightly for robustness
                            component.right_edge_pin_indices.size() >= 2 &&
                            component.top_edge_pin_indices.size() >= 2 &&
                            component.bottom_edge_pin_indices.size() >= 2);

        bool high_pins_top_or_bottom = component.top_edge_pin_indices.size() >= 8 || component.bottom_edge_pin_indices.size() >= 8;
        bool high_pins_left_or_right = component.left_edge_pin_indices.size() >= 8 || component.right_edge_pin_indices.size() >= 8;

        component.is_connector = (high_pins_top_or_bottom || high_pins_left_or_right) && !component.is_qfp;
        if (component.is_qfp)
            component.is_connector = false;

        // Pin Orientation Logic
        for (size_t pin_idx = 0; pin_idx < component.pins.size(); ++pin_idx)
        {
            Pin &pin = component.pins[pin_idx];
            LocalEdge assigned_edge = pin_to_local_edge_map[pin_idx]; // Use the new map

            bool is_square_or_circle = std::abs(pin.initial_width - pin.initial_height) < 1e-3;

            if (is_square_or_circle)
            { // Covers single pin case too if it's square/circle
                pin.orientation = PinOrientation::Natural;
                continue;
            }
            // Single non-square/circle pins handled by general logic or INTERIOR if not on edge

            component.is_two_pad = (component.pins.size() == 2); // Recalc here or ensure it's set earlier
            if (component.is_two_pad)
            {                                            // Specific logic for two-pad non-square/circle
                const Pin &p0_const = component.pins[0]; // Assuming pin_idx is 0 or 1
                const Pin &p1_const = component.pins[(pin_idx == 0) ? 1 : 0];
                BoardPoint2D local_p0_pos = local_pin_positions[pin_idx];                // current pin
                BoardPoint2D local_p1_pos = local_pin_positions[(pin_idx == 0) ? 1 : 0]; // other pin

                // If pins are more separated horizontally than vertically in local frame, they form a horizontal pair
                bool pins_form_horizontal_pair = std::abs(local_p0_pos.x - local_p1_pos.x) > std::abs(local_p0_pos.y - local_p1_pos.y);
                // For a horizontal pair of pins, orient pins vertically
                // For a vertical pair of pins, orient pins horizontally
                pin.orientation = pins_form_horizontal_pair ? PinOrientation::Vertical : PinOrientation::Horizontal;
            }
            else if (component.is_qfp)
            {
                if (assigned_edge == LocalEdge::TOP || assigned_edge == LocalEdge::BOTTOM)
                {
                    pin.orientation = PinOrientation::Vertical;
                }
                else if (assigned_edge == LocalEdge::LEFT || assigned_edge == LocalEdge::RIGHT)
                {
                    pin.orientation = PinOrientation::Horizontal;
                }
                else
                { // Interior QFP pin? Fallback to natural aspect
                    pin.orientation = (pin.initial_height > pin.initial_width) ? PinOrientation::Vertical : PinOrientation::Horizontal;
                }
            }
            else if (component.is_connector)
            { // QFP and Connector are mutually exclusive
                if (component.is_wide_component)
                {
                    pin.orientation = PinOrientation::Vertical;
                }
                else if (component.is_tall_component)
                {
                    pin.orientation = PinOrientation::Horizontal;
                }
                else
                { // Roughly square connector
                    if (assigned_edge == LocalEdge::TOP || assigned_edge == LocalEdge::BOTTOM)
                        pin.orientation = PinOrientation::Vertical;
                    else if (assigned_edge == LocalEdge::LEFT || assigned_edge == LocalEdge::RIGHT)
                        pin.orientation = PinOrientation::Horizontal;
                    else
                        pin.orientation = (pin.initial_height > pin.initial_width) ? PinOrientation::Vertical : PinOrientation::Horizontal;
                }
            }
            else
            { // General multi-pin components (not QFP, not Connector, not 2-pad, not single_pin handled by square/circle)
                switch (assigned_edge)
                {
                case LocalEdge::LEFT:
                case LocalEdge::RIGHT:
                    // Align short side parallel to Y-axis (edge is vertical)
                    // Pin's oriented height should be its short side. Achieved with Horizontal.
                    pin.orientation = PinOrientation::Horizontal;
                    break;
                case LocalEdge::TOP:
                case LocalEdge::BOTTOM:
                    // Align short side parallel to X-axis (edge is horizontal)
                    // Pin's oriented width should be its short side. Achieved with Vertical.
                    pin.orientation = PinOrientation::Vertical;
                    break;
                case LocalEdge::INTERIOR:
                case LocalEdge::UNKNOWN:
                    pin.orientation = PinOrientation::Natural;
                    break;
                default:
                    // Component is roughly square or aspect unknown
                    pin.orientation = PinOrientation::Natural;

                    break;
                }
            }
        }
        // Note: The alignment flags (e.g., left_pins_vertically_aligned) and detectPinAlignment are removed
        // as the new orientation logic is more direct based on edge type or component type.
        // The "Initial Problem Check & Tentative Flip" from previous refactoring is also omitted for now,
        // relying on Pass 2 and 3 for corrections.
    }

    void OrientationProcessor::secondPass_CheckOverlapsAndBoundaries(Component &component, double component_rotation_radians)
    {
        double local_rotation_rad = -component_rotation_radians;

        if (component.pins.size() <= 1)
            return;

        double comp_min_x = std::numeric_limits<double>::max();
        double comp_max_x = std::numeric_limits<double>::lowest();
        double comp_min_y = std::numeric_limits<double>::max();
        double comp_max_y = std::numeric_limits<double>::lowest();

        if (!component.graphical_elements.empty())
        {
            for (const auto &seg : component.graphical_elements)
            {
                BoardPoint2D local_start = rotatePoint(seg.start.x, seg.start.y, local_rotation_rad);
                BoardPoint2D local_end = rotatePoint(seg.end.x, seg.end.y, local_rotation_rad);
                comp_min_x = std::min({comp_min_x, local_start.x, local_end.x});
                comp_max_x = std::max({comp_max_x, local_start.x, local_end.x});
                comp_min_y = std::min({comp_min_y, local_start.y, local_end.y});
                comp_max_y = std::max({comp_max_y, local_start.y, local_end.y});
            }
            double margin_x = (comp_max_x - comp_min_x) * 0.05;
            double margin_y = (comp_max_y - comp_min_y) * 0.05;
            if (margin_x < 1e-6)
                margin_x = 0.1;
            if (margin_y < 1e-6)
                margin_y = 0.1;
            comp_min_x -= margin_x;
            comp_max_x += margin_x;
            comp_min_y -= margin_y;
            comp_max_y += margin_y;
        }
        else if (component.width > 0 && component.height > 0)
        {
            // Assume component.width/height define the AABB in its local, unrotated frame, centered at (0,0) locally.
            // Pin coordinates are relative to component anchor, which is our local (0,0) after rotation.
            comp_min_x = -component.width / 2.0;
            comp_max_x = component.width / 2.0;
            comp_min_y = -component.height / 2.0;
            comp_max_y = component.height / 2.0;
        }
        else
        {
            // Fallback to pin bounding box (calculated in local frame in pass 1)
            comp_min_x = component.pin_bbox_min_x;
            comp_max_x = component.pin_bbox_max_x;
            comp_min_y = component.pin_bbox_min_y;
            comp_max_y = component.pin_bbox_max_y;
            double tol = 0.1;
            comp_min_x -= tol;
            comp_max_x += tol;
            comp_min_y -= tol;
            comp_max_y += tol;
        }

        for (size_t pin_idx = 0; pin_idx < component.pins.size(); ++pin_idx)
        {
            Pin &current_pin = component.pins[pin_idx];
            BoardPoint2D local_current_pin_pos = rotatePoint(current_pin.x_coord, current_pin.y_coord, local_rotation_rad);

            double current_w, current_h;
            getOrientedPinDimensions(current_pin, current_w, current_h); // Uses current_pin.scale_factor
            double current_half_w = current_w / 2.0;
            double current_half_h = current_h / 2.0;

            bool extends_beyond_bounds =
                (local_current_pin_pos.x - current_half_w < comp_min_x) ||
                (local_current_pin_pos.x + current_half_w > comp_max_x) ||
                (local_current_pin_pos.y - current_half_h < comp_min_y) ||
                (local_current_pin_pos.y + current_half_h > comp_max_y);

            bool overlaps_other_pin = false;
            for (size_t other_idx = 0; other_idx < component.pins.size(); ++other_idx)
            {
                if (pin_idx == other_idx)
                    continue;
                const Pin &other_pin_const = component.pins[other_idx];
                BoardPoint2D local_other_pin_pos = rotatePoint(other_pin_const.x_coord, other_pin_const.y_coord, local_rotation_rad);

                double other_w, other_h;
                getOrientedPinDimensions(other_pin_const, other_w, other_h); // Uses other_pin.scale_factor
                double other_half_w = other_w / 2.0;
                double other_half_h = other_h / 2.0;

                bool x_overlap = std::abs(local_current_pin_pos.x - local_other_pin_pos.x) < (current_half_w + other_half_w);
                bool y_overlap = std::abs(local_current_pin_pos.y - local_other_pin_pos.y) < (current_half_h + other_half_h);

                if (x_overlap && y_overlap)
                {
                    overlaps_other_pin = true;
                    break;
                }
            }

            // Store original state for decision making and scaling
            PinOrientation initial_pin_orientation = current_pin.orientation;
            bool problem_detected = extends_beyond_bounds || overlaps_other_pin;

            if (problem_detected)
            {
                // For Natural square/circle pins, flipping is not typically done. Scaling will be handled later if needed.
                bool is_natural_square_circle = (initial_pin_orientation == PinOrientation::Natural && std::abs(current_pin.initial_width - current_pin.initial_height) < 1e-3);

                if (!is_natural_square_circle)
                {
                    PinOrientation opposite_orientation;
                    if (initial_pin_orientation == PinOrientation::Vertical)
                        opposite_orientation = PinOrientation::Horizontal;
                    else if (initial_pin_orientation == PinOrientation::Horizontal)
                        opposite_orientation = PinOrientation::Vertical;
                    else // Natural but not square/circle, or some other state
                    {
                        if (current_pin.initial_width > current_pin.initial_height)      // Naturally wider
                            opposite_orientation = PinOrientation::Vertical;             // Try to make it tall
                        else if (current_pin.initial_height > current_pin.initial_width) // Naturally taller
                            opposite_orientation = PinOrientation::Horizontal;           // Try to make it wide
                        else                                                             // Still effectively square/circle from initial dimensions
                            opposite_orientation = PinOrientation::Horizontal;           // Default opposite
                    }

                    // Test opposite orientation using the pin's current scale factor
                    Pin test_pin_for_opposite = current_pin; // Copies current scale_factor
                    test_pin_for_opposite.orientation = opposite_orientation;
                    double opposite_w, opposite_h;
                    getOrientedPinDimensions(test_pin_for_opposite, opposite_w, opposite_h);
                    double opposite_half_w = opposite_w / 2.0;
                    double opposite_half_h = opposite_h / 2.0;

                    bool opposite_extends =
                        (local_current_pin_pos.x - opposite_half_w < comp_min_x) ||
                        (local_current_pin_pos.x + opposite_half_w > comp_max_x) ||
                        (local_current_pin_pos.y - opposite_half_h < comp_min_y) ||
                        (local_current_pin_pos.y + opposite_half_h > comp_max_y);

                    bool opposite_overlaps = false;
                    for (size_t other_idx = 0; other_idx < component.pins.size(); ++other_idx)
                    {
                        if (pin_idx == other_idx)
                            continue;
                        const Pin &other_pin_const_check = component.pins[other_idx]; // Renamed to avoid conflict
                        BoardPoint2D local_other_pin_pos_check = rotatePoint(other_pin_const_check.x_coord, other_pin_const_check.y_coord, local_rotation_rad);

                        double other_w_check, other_h_check;
                        getOrientedPinDimensions(other_pin_const_check, other_w_check, other_h_check);
                        double other_half_w_check = other_w_check / 2.0;
                        double other_half_h_check = other_h_check / 2.0;

                        bool x_overlap_opp = std::abs(local_current_pin_pos.x - local_other_pin_pos_check.x) < (opposite_half_w + other_half_w_check);
                        bool y_overlap_opp = std::abs(local_current_pin_pos.y - local_other_pin_pos_check.y) < (opposite_half_h + other_half_h_check);

                        if (x_overlap_opp && y_overlap_opp)
                        {
                            opposite_overlaps = true;
                            break;
                        }
                    }

                    // Decision to flip:
                    // 1. If opposite orientation is perfect (no extension, no overlap), flip.
                    if (!opposite_extends && !opposite_overlaps)
                    {
                        printf("[secondPass]: Pin %s: Opposite orientation is perfect. Flipping from %s to %s\n", current_pin.pin_name.c_str(), current_pin.orientation_name().c_str(), opposite_orientation == PinOrientation::Vertical ? "Vertical" : "Horizontal");
                        current_pin.orientation = opposite_orientation;
                    }
                }
            } // End of problem_detected block (flipping logic)
        }
    }

    void OrientationProcessor::thirdPass_FinalBoundaryCheck(Component &component, double component_rotation_radians)
    {
        double local_rotation_rad = -component_rotation_radians;

        if (component.pins.empty())
            return;

        double comp_min_x = std::numeric_limits<double>::max();
        double comp_max_x = std::numeric_limits<double>::lowest();
        double comp_min_y = std::numeric_limits<double>::max();
        double comp_max_y = std::numeric_limits<double>::lowest();
        bool boundary_defined = false;

        if (!component.graphical_elements.empty())
        {
            for (const auto &seg : component.graphical_elements)
            {
                BoardPoint2D local_start = rotatePoint(seg.start.x, seg.start.y, local_rotation_rad);
                BoardPoint2D local_end = rotatePoint(seg.end.x, seg.end.y, local_rotation_rad);
                comp_min_x = std::min({comp_min_x, local_start.x, local_end.x});
                comp_max_x = std::max({comp_max_x, local_start.x, local_end.x});
                comp_min_y = std::min({comp_min_y, local_start.y, local_end.y});
                comp_max_y = std::max({comp_max_y, local_start.y, local_end.y});
            }
            boundary_defined = true;
        }
        else if (component.width > 0 && component.height > 0)
        {
            comp_min_x = -component.width / 2.0;
            comp_max_x = component.width / 2.0;
            comp_min_y = -component.height / 2.0;
            comp_max_y = component.height / 2.0;
            boundary_defined = true;
        }

        if (!boundary_defined)
            return;

        double margin_x = (comp_max_x - comp_min_x) * 0.001;
        double margin_y = (comp_max_y - comp_min_y) * 0.001;
        margin_x = std::max(margin_x, 1e-4);
        margin_y = std::max(margin_y, 1e-4);

        for (Pin &pin : component.pins)
        {
            if (pin.orientation == PinOrientation::Natural)
            {
                continue;
            }

            BoardPoint2D local_pin_pos = rotatePoint(pin.x_coord, pin.y_coord, local_rotation_rad);

            // Calculate current pin dimensions (already scaled from pass 2) and extents in local frame
            double current_w, current_h;
            getOrientedPinDimensions(pin, current_w, current_h); // Uses pin.scale_factor
            double current_half_w = current_w / 2.0;
            double current_half_h = current_h / 2.0;

            double pin_min_x_local = local_pin_pos.x - current_half_w;
            double pin_max_x_local = local_pin_pos.x + current_half_w;
            double pin_min_y_local = local_pin_pos.y - current_half_h;
            double pin_max_y_local = local_pin_pos.y + current_half_h;

            bool extends_significantly =
                (pin_min_x_local < comp_min_x - margin_x) ||
                (pin_max_x_local > comp_max_x + margin_x) ||
                (pin_min_y_local < comp_min_y - margin_y) ||
                (pin_max_y_local > comp_max_y + margin_y);

            if (extends_significantly)
            {
                printf("[thirdPass]: Pin %s: Extends beyond boundaries. On %s. Current orientation: %s\n", pin.pin_name.c_str(), pin.edge_name().c_str(), pin.orientation_name().c_str());
                pin.debug_color = BLRgba32(255, 0, 0, 200);
                double current_extension = 0.0;
                if (pin_min_x_local < comp_min_x)
                    current_extension += (comp_min_x - pin_min_x_local);
                if (pin_max_x_local > comp_max_x)
                    current_extension += (pin_max_x_local - comp_max_x);
                if (pin_min_y_local < comp_min_y)
                    current_extension += (comp_min_y - pin_min_y_local);
                if (pin_max_y_local > comp_max_y)
                    current_extension += (pin_max_y_local - comp_max_y);

                PinOrientation opposite_orientation;
                if (pin.orientation == PinOrientation::Vertical)
                    opposite_orientation = PinOrientation::Horizontal;
                else if (pin.orientation == PinOrientation::Horizontal)
                    opposite_orientation = PinOrientation::Vertical;
                else
                { // Should not happen due to Natural check, but as a fallback
                    opposite_orientation = (pin.initial_width > pin.initial_height) ? PinOrientation::Vertical : PinOrientation::Horizontal;
                }

                Pin test_pin_opposite = pin; // Copies current scale_factor too
                test_pin_opposite.orientation = opposite_orientation;
                double opposite_w, opposite_h;
                getOrientedPinDimensions(test_pin_opposite, opposite_w, opposite_h);
                double opposite_half_w = opposite_w / 2.0;
                double opposite_half_h = opposite_h / 2.0;

                double opposite_pin_min_x_local = local_pin_pos.x - opposite_half_w;
                double opposite_pin_max_x_local = local_pin_pos.x + opposite_half_w;
                double opposite_pin_min_y_local = local_pin_pos.y - opposite_half_h;
                double opposite_pin_max_y_local = local_pin_pos.y + opposite_half_h;

                double opposite_extension = 0.0;
                if (opposite_pin_min_x_local < comp_min_x)
                    opposite_extension += (comp_min_x - opposite_pin_min_x_local);
                if (opposite_pin_max_x_local > comp_max_x)
                    opposite_extension += (opposite_pin_max_x_local - comp_max_x);
                if (opposite_pin_min_y_local < comp_min_y)
                    opposite_extension += (comp_min_y - opposite_pin_min_y_local);
                if (opposite_pin_max_y_local > comp_max_y)
                    opposite_extension += (opposite_pin_max_y_local - comp_max_y);

                if (opposite_extension < current_extension)
                {
                    pin.orientation = opposite_orientation;
                }
            }
        }
    }

    void OrientationProcessor::getOrientedPinDimensions(const Pin &pin, double &width, double &height)
    {
        width = pin.initial_width;
        height = pin.initial_height;

        if (pin.orientation == PinOrientation::Vertical)
        {
            if (width > height)
            { // If natural/initial was horizontal, swap for vertical
                std::swap(width, height);
            }
        }
        else if (pin.orientation == PinOrientation::Horizontal)
        {
            if (height > width)
            { // If natural/initial was vertical, swap for horizontal
                std::swap(width, height);
            }
        }
        // For PinOrientation::Natural, width and height remain as initial_width and initial_height (before scaling)
    }

    double OrientationProcessor::calculateComponentRotation(const Component &component)
    {
        if (component.graphical_elements.empty())
        {
            return 0.0; // No segments to determine rotation
        }

        std::map<int, double> angle_histogram; // Angle (degrees, rounded) to cumulative length
        const double ANGLE_RESOLUTION = 5.0;   // Degrees

        for (const auto &seg : component.graphical_elements)
        {
            double dx = seg.end.x - seg.start.x;
            double dy = seg.end.y - seg.start.y;
            double length = std::sqrt(dx * dx + dy * dy);
            if (length < 1e-6)
                continue; // Skip zero-length segments

            double angle_rad = std::atan2(dy, dx);
            double angle_deg = angle_rad * 180.0 / M_PI;

            // Normalize angle to [0, 180) as orientation is often about an axis
            // A line at 0 deg is same orientation as 180 deg for component body outline
            angle_deg = std::fmod(angle_deg, 180.0);
            if (angle_deg < 0)
                angle_deg += 180.0;

            int rounded_angle_key = static_cast<int>(std::round(angle_deg / ANGLE_RESOLUTION) * ANGLE_RESOLUTION);
            rounded_angle_key = rounded_angle_key % 180; // Ensure key is within [0, 175]

            angle_histogram[rounded_angle_key] += length;
        }

        if (angle_histogram.empty())
        {
            return 0.0;
        }

        // Find the angle with the maximum cumulative length
        double max_length = 0.0;
        int dominant_angle_deg = 0;
        for (const auto &pair : angle_histogram)
        {
            if (pair.second > max_length)
            {
                max_length = pair.second;
                dominant_angle_deg = pair.first;
            }
        }
        // Also check the perpendicular angle's popularity (+90 deg)
        // This helps distinguish between a long thin rectangle and a square-ish one with many segments along one axis.
        int perpendicular_angle_key = (dominant_angle_deg + 90) % 180;
        double perpendicular_length = 0.0;
        if (angle_histogram.count(perpendicular_angle_key))
        {
            perpendicular_length = angle_histogram[perpendicular_angle_key];
        }

        // If the dominant angle and its perpendicular are very close in length (e.g. for a square)
        // and the dominant angle is close to 45, 135, etc., it might be ambiguous.
        // For now, we primarily trust the dominant angle. More sophisticated logic could be added
        // if components are often highly symmetrical with ambiguous longest sides (e.g. perfect squares rotated by 45 deg).
        // The original request implied we can find two matching sides for rectangular/square components.
        // This histogram approach is more general.
        // If the component is truly rectangular, two dominant angles (primary and perpendicular) should emerge.

        // If component.width and component.height are available and reliable, and assuming they define the primary axes:
        // If component.width > component.height, the dominant angle found is likely along the width.
        // If component.height > component.width, the dominant angle found is likely along the height.
        // This could be used to adjust if the angle found is for the shorter side of a clear rectangle.
        // For now, we just return the angle of the longest cumulative segment lengths.

        return static_cast<double>(dominant_angle_deg) * M_PI / 180.0;
    }

} // namespace PCBProcessing

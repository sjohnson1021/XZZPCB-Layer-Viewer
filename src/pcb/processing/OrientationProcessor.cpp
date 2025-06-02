#include "OrientationProcessor.hpp"

#include <algorithm>  // For std::min, std::max, std::sort, std::find
#include <cmath>      // For std::sqrt, std::atan2, std::abs, std::cos, std::sin, M_PI
#include <limits>     // For std::numeric_limits
#include <map>        // For std::map in calculateComponentRotation

#include "pcb/Board.hpp"  // Required for Board class definition
#include "pcb/elements/Component.hpp"
#include "pcb/elements/Pin.hpp"

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

// For debug printing, if needed
// #include <cstdio>

namespace PCBProcessing
{

struct LocalLineSegment {
    BoardPoint2D start;
    BoardPoint2D end;
    double angle_rad_local;  // Angle in the component\'s local, axis-aligned frame
    double length;

    LocalLineSegment(BoardPoint2D s, BoardPoint2D e) : start(s), end(e)
    {
        double dx = end.x - start.x;
        double dy = end.y - start.y;
        length = std::sqrt(dx * dx + dy * dy);
        if (length > 1e-9) {  // Avoid division by zero or atan2(0,0) issues
            angle_rad_local = std::atan2(dy, dx);
        } else {
            angle_rad_local = 0.0;
        }
    }
};

// Helper function to check for AABB intersection
static bool areRectsIntersecting(const BLRect& r1, const BLRect& r2, double tolerance = 1e-6)
{
    // Add tolerance to the checks
    return r1.x < r2.x + r2.w + tolerance && r1.x + r1.w > r2.x - tolerance && r1.y < r2.y + r2.h + tolerance && r1.y + r1.h > r2.y - tolerance;
}

// Helper function to rotate a point around the origin (0,0)
static inline Vec2 rotatePoint(double x, double y, double angle_rad)
{
    double cos_a = std::cos(angle_rad);
    double sin_a = std::sin(angle_rad);
    return {x * cos_a - y * sin_a, x * sin_a + y * cos_a};
}

void OrientationProcessor::processBoard(Board& board)
{
    // Define tolerance for various floating point comparisons
    constexpr double epsilon = 1e-4;  // General small number for floating point comparisons
    // constexpr double pin_alignment_tolerance = 0.5; // Max distance for pins to be considered aligned (mm or current units)
    // constexpr double edge_proximity_tolerance = 0.1; // How close a pin needs to be to an edge (mm)
    for (auto& component : board.m_components) {}
    // First Pass: Analyze individual components
    if (board.m_components.empty()) {
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
        printf("[OrientationProcessor]: No components found in the board.\\n");
#endif
        return;
    }

#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
    printf("[OrientationProcessor]: Starting First Pass - Analyzing %zu components.\\n", board.m_components.size());
#endif
    for (Component& component : board.m_components)  // Corrected to m_components
    {
        if (component.pins.empty())
            continue;
        firstPass_AnalyzeComponent(component, epsilon);
    }

    // Second Pass: Check for overlaps and boundary conditions within components (more detailed heuristics)
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
    printf("[OrientationProcessor]: Starting Second Pass - Checking overlaps and boundaries for %zu components.\\n", board.m_components.size());
#endif
    for (Component& component : board.m_components)  // Corrected to m_components
    {
        if (component.pins.empty())
            continue;
        secondPass_CheckOverlapsAndBoundaries(component, epsilon);
    }

    // Third Pass: Final adjustments, especially for pins near component edges
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
    printf("[OrientationProcessor]: Starting Third Pass - Final boundary checks for %zu components.\\n", board.m_components.size());
#endif
    for (Component& component : board.m_components)  // Corrected to m_components
    {
        if (component.pins.empty())
            continue;
        thirdPass_FinalBoundaryCheck(component, epsilon);
    }

#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
    printf("[OrientationProcessor]: Pin orientation processing complete for all components.\\n");
    // Example of printing final orientations for verification:
    for (const auto& component_ptr : board.m_components)  // Assuming m_components stores Component objects directly or smart pointers
    {
        // If m_components stores smart pointers, dereference component_ptr
        // For example, if it's std::vector<std::unique_ptr<Component>>:
        // const Component& component = *component_ptr;
        // If it's std::vector<Component>:
        const Component& component = component_ptr;  // If m_components is vector<Component> directly as implied by loop earlier

        if (component.pins.empty())
            continue;
        printf("Component: %s\\n", component.reference_designator.c_str());
        for (const auto& pin_ptr : component.pins)  // Iterate through unique_ptrs
        {
            if (pin_ptr) {  // Check if unique_ptr is not null
                auto pinOrientationToString = [](PinOrientation o) {
                    if (o == PinOrientation::Vertical)
                        return "Vertical";
                    if (o == PinOrientation::Horizontal)
                        return "Horizontal";
                    return "Natural";
                };
                printf("  Pin: %s, Orientation: %s\\n", pin_ptr->pin_name.c_str(), pinOrientationToString(pin_ptr->orientation));
            }
        }
    }
#endif
}

void OrientationProcessor::calculateInitialPinDimensions(Pin& pin)
{
    // Initialize pin.width and pin.height based on pin.pad_shape

    std::visit(
        [&](const auto& shape) {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, CirclePad>) {
                pin.width = shape.radius * 2.0;
                pin.height = shape.radius * 2.0;
            } else if constexpr (std::is_same_v<T, RectanglePad>) {
                pin.width = shape.width;
                pin.height = shape.height;
                // } else if constexpr (std::is_same_v<T, SquarePad>) { // Assuming SquarePad is distinct or handled by RectanglePad
                //     pin.width = shape.side_length;
                //     pin.height = shape.side_length;
            } else if constexpr (std::is_same_v<T, CapsulePad>) {
                pin.width = shape.width;    // Total width of capsule
                pin.height = shape.height;  // Diameter / height part
            }
        },
        pin.pad_shape);
    pin.short_side = std::min(pin.width, pin.height);
    pin.long_side = std::max(pin.width, pin.height);
    // Ensure natural orientation starts with width > height if applicable (e.g. for rectangles)
    // The rendering logic might swap them based on PinOrientation, this sets the baseline.
    // if (pin.width < pin.height) {
    //     std::swap(pin.width, pin.height);
    // }
}

void OrientationProcessor::firstPass_AnalyzeComponent(Component& component, double tolerance)
{
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
    printf("[firstPass]: Analyzing component %s with %zu pins.\\n", component.reference_designator.c_str(), component.pins.size());
#endif
    if (component.pins.empty())
        return;

    calculatePinBoundingBox(component, tolerance);

    // Simple checks based on pin count
    component.is_single_pin = (component.pins.size() == 1);
    component.is_two_pad = (component.pins.size() == 2);

    if (component.is_single_pin && !component.pins.empty()) {
        Pin& pin = *component.pins[0];              // Dereference unique_ptr
        pin.orientation = PinOrientation::Natural;  // Typically no specific orientation needed
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
        printf("  Pin %s (single): Set to Natural.\\n", pin.pin_name.c_str());
#endif
        return;  // Nothing more to do for single pin components
    }

    if (component.is_two_pad && !component.pins.empty()) {
        Pin& p0 = *component.pins[0];
        Pin& p1 = *component.pins[1];
        double dx = p1.x_coord - p0.x_coord;
        double dy = p1.y_coord - p0.y_coord;
        if (std::abs(dx) > std::abs(dy) * 1.1) {
            p0.orientation = PinOrientation::Vertical;
            p1.orientation = PinOrientation::Vertical;
            p0.SetDimensionsForOrientation();
            p1.SetDimensionsForOrientation();
        } else if (std::abs(dy) > std::abs(dx) * 1.1) {
            p0.orientation = PinOrientation::Horizontal;
            p1.orientation = PinOrientation::Horizontal;
            p0.SetDimensionsForOrientation();
            p1.SetDimensionsForOrientation();
        } else {
            p0.orientation = PinOrientation::Natural;
            p1.orientation = PinOrientation::Natural;
            p0.SetDimensionsForOrientation();
            p1.SetDimensionsForOrientation();
        }
    }

    // Further analysis for multi-pin components
    double pin_bbox_width = component.pin_bbox_max_x - component.pin_bbox_min_x;
    double pin_bbox_height = component.pin_bbox_max_y - component.pin_bbox_min_y;

    component.is_wide_component = pin_bbox_width > pin_bbox_height * 1.25;  // Heuristic: 25% wider
    component.is_tall_component = pin_bbox_height > pin_bbox_width * 1.25;  // Heuristic: 25% taller

    // Collect local pin positions relative to component center and rotated to align with component axes
    std::vector<Vec2> local_pin_positions(component.pins.size());
    double local_rotation_rad = -component.rotation * (M_PI / 180.0);  // Inverse rotation, Changed kPi to M_PI

    for (size_t i = 0; i < component.pins.size(); ++i) {
        if (!component.pins[i])
            continue;
        // Access x_coord and y_coord from the Pin object pointed to by unique_ptr
        // rotatePoint now returns Vec2, so direct assignment is fine.
        local_pin_positions[i] = rotatePoint(component.pins[i]->x_coord - component.center_x, component.pins[i]->y_coord - component.center_y, local_rotation_rad);
    }

    // Classify pins to edges (Left, Right, Top, Bottom, Interior)
    // This is a simplified classification based on the pin bounding box
    component.left_edge_pin_indices.clear();
    component.right_edge_pin_indices.clear();
    component.top_edge_pin_indices.clear();
    component.bottom_edge_pin_indices.clear();
    std::vector<LocalEdge> pin_to_local_edge_map(component.pins.size(), LocalEdge::INTERIOR);

    double avg_pin_short_side = 0;
    int valid_pins_for_avg = 0;
    for (const auto& p_ptr : component.pins) {
        if (p_ptr) {
            avg_pin_short_side += p_ptr->short_side;  // Access via ->
            valid_pins_for_avg++;
        }
    }
    if (valid_pins_for_avg > 0)
        avg_pin_short_side /= valid_pins_for_avg;
    else
        avg_pin_short_side = 0.1;  // Default if no valid pins

    // ... (rest of edge classification logic, ensuring pin_ptr->member access)

    // Example of edge classification (needs to be completed and refined)
    // Tolerance for being "on" an edge, could be related to avg_pin_short_side
    double edge_tolerance = avg_pin_short_side * 1.2;

    for (size_t i = 0; i < component.pins.size(); ++i) {
        if (!component.pins[i])
            continue;                         // Skip null unique_ptrs
        const Pin& pin = *component.pins[i];  // Dereference for easier access

        // Use local_pin_positions which are relative to component center and aligned
        Vec2 local_pos = local_pin_positions[i];

        // Calculate component-local boundaries (relative to component center 0,0)
        double local_left_edge = -pin_bbox_width / 2.5;
        double local_right_edge = pin_bbox_width / 2.5;
        double local_top_edge = -pin_bbox_height / 2.5;    // Top is often min Y in local graphics
        double local_bottom_edge = pin_bbox_height / 2.5;  // Bottom is often max Y

        if (std::abs(local_pos.x - local_left_edge) < edge_tolerance) {
            pin_to_local_edge_map[i] = LocalEdge::LEFT;
            component.left_edge_pin_indices.push_back(i);
        } else if (std::abs(local_pos.x - local_right_edge) < edge_tolerance) {
            pin_to_local_edge_map[i] = LocalEdge::RIGHT;
            component.right_edge_pin_indices.push_back(i);
        } else if (std::abs(local_pos.y - local_top_edge) < edge_tolerance) {
            pin_to_local_edge_map[i] = LocalEdge::TOP;
            component.top_edge_pin_indices.push_back(i);
        } else if (std::abs(local_pos.y - local_bottom_edge) < edge_tolerance) {
            pin_to_local_edge_map[i] = LocalEdge::BOTTOM;
            component.bottom_edge_pin_indices.push_back(i);
        } else {
            pin_to_local_edge_map[i] = LocalEdge::INTERIOR;
        }
    }
    for (size_t i = 0; i < component.pins.size(); ++i) {
        if (component.pins[i]) {                                       // Check if unique_ptr is not null
            component.pins[i]->local_edge = pin_to_local_edge_map[i];  // Store on pin itself, access via ->
        }
    }

    // Default orientation based on component shape and pin distribution
    for (auto& pin_ptr : component.pins)  // Iterate unique_ptr
    {
        if (!pin_ptr)
            continue;
        Pin& pin = *pin_ptr;  // Dereference

        if (pin.local_edge == LocalEdge::LEFT || pin.local_edge == LocalEdge::RIGHT) {
            pin.orientation = PinOrientation::Horizontal;
            pin.SetDimensionsForOrientation();
        } else if (pin.local_edge == LocalEdge::TOP || pin.local_edge == LocalEdge::BOTTOM) {
            pin.orientation = PinOrientation::Vertical;
            pin.SetDimensionsForOrientation();
        } else {  // Interior or unclassified
            // If component is distinctly wide or tall, orient pins along the shorter dimension of the component
            if (component.is_wide_component) {
                pin.orientation = PinOrientation::Vertical;  // Pins run vertically on a wide component
                pin.SetDimensionsForOrientation();
            } else if (component.is_tall_component) {
                pin.orientation = PinOrientation::Horizontal;  // Pins run horizontally on a tall component
                pin.SetDimensionsForOrientation();
            } else {
                pin.orientation = PinOrientation::Natural;  // Default for square-ish or interior
                pin.SetDimensionsForOrientation();
            }
        }
    }

    // Special case for 2-pin components (resistors, capacitors, diodes)
    if (component.is_two_pad && component.pins.size() == 2) {
        if (!component.pins[0] || !component.pins[1])
            return;  // Should not happen if size is 2

        Pin& p0 = *component.pins[0];  // Dereference
        Pin& p1 = *component.pins[1];  // Dereference

        double dx = p1.x_coord - p0.x_coord;
        double dy = p1.y_coord - p0.y_coord;

        if (std::abs(dx) > std::abs(dy) * 1.2)  // More horizontal than vertical (allow some tolerance)
        {
            p0.orientation = PinOrientation::Horizontal;
            p1.orientation = PinOrientation::Horizontal;
            p0.SetDimensionsForOrientation();
            p1.SetDimensionsForOrientation();
        } else if (std::abs(dy) > std::abs(dx) * 1.2)  // More vertical than horizontal
        {
            p0.orientation = PinOrientation::Vertical;
            p1.orientation = PinOrientation::Vertical;
            p0.SetDimensionsForOrientation();
            p1.SetDimensionsForOrientation();
        } else {  // Roughly diagonal or same point, default based on component shape or keep Natural
            PinOrientation default_orientation = PinOrientation::Natural;
            if (component.is_wide_component)
                default_orientation = PinOrientation::Vertical;
            else if (component.is_tall_component)
                default_orientation = PinOrientation::Horizontal;
            p0.orientation = default_orientation;
            p1.orientation = default_orientation;
            p0.SetDimensionsForOrientation();
            p1.SetDimensionsForOrientation();
        }
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
        auto pinOrientationToString = [](PinOrientation o) {
            if (o == PinOrientation::Vertical)
                return "Vertical";
            if (o == PinOrientation::Horizontal)
                return "Horizontal";
            return "Natural";
        };
        printf("  Component %s (2-pin): p0 (%s) orientation: %s, p1 (%s) orientation: %s\\n",
               component.reference_designator.c_str(),
               p0.pin_name.c_str(),
               pinOrientationToString(p0.orientation),
               p1.pin_name.c_str(),
               pinOrientationToString(p1.orientation));
#endif
    }
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
    printf("[firstPass]: Finished analyzing component %s.\\n", component.reference_designator.c_str());
    auto localEdgeToString = [](LocalEdge le) {
        if (le == LocalEdge::LEFT)
            return "Left";
        if (le == LocalEdge::RIGHT)
            return "Right";
        if (le == LocalEdge::TOP)
            return "Top";
        if (le == LocalEdge::BOTTOM)
            return "Bottom";
        if (le == LocalEdge::INTERIOR)
            return "Interior";
        return "Unknown";
    };
    for (size_t i = 0; i < component.pins.size(); ++i) {
        if (component.pins[i]) {
            printf("  Pin %s (%s): Initial Orientation: %s\\n",
                   component.pins[i]->pin_name.c_str(),
                   localEdgeToString(component.pins[i]->local_edge),
                   (component.pins[i]->orientation == PinOrientation::Vertical ? "Vertical" : (component.pins[i]->orientation == PinOrientation::Horizontal ? "Horizontal" : "Natural")));
        }
    }
#endif
}

void OrientationProcessor::secondPass_CheckOverlapsAndBoundaries(Component& component, double tolerance)
{
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
    printf("[secondPass]: Checking component %s\n", component.reference_designator.c_str());
#endif

    // Helper lambdas for string conversion
    auto pinOrientationToString = [](PinOrientation o) {
        if (o == PinOrientation::Vertical)
            return "Vertical";
        if (o == PinOrientation::Horizontal)
            return "Horizontal";
        return "Natural";
    };

    for (size_t pin_idx = 0; pin_idx < component.pins.size(); ++pin_idx) {
        if (!component.pins[pin_idx])
            continue;
        Pin& current_pin = *component.pins[pin_idx];  // Dereference

        // Calculate current pin's bounding box (simplified, assuming centered rectangle)
        double current_w = current_pin.width;
        double current_h = current_pin.height;
        BLRect current_pin_bbox = BLRect(current_pin.x_coord - current_w / 2.0, current_pin.y_coord - current_h / 2.0, current_w, current_h);

        // Check against other pins in the same component
        for (size_t other_idx = 0; other_idx < component.pins.size(); ++other_idx) {
            if (pin_idx == other_idx || !component.pins[other_idx])
                continue;

            const Pin& other_pin_const = *component.pins[other_idx];  // Dereference

            double other_w = other_pin_const.width;
            double other_h = other_pin_const.height;
            BLRect other_pin_bbox = BLRect(other_pin_const.x_coord - other_w / 2.0, other_pin_const.y_coord - other_h / 2.0, other_w, other_h);

            if (areRectsIntersecting(current_pin_bbox, other_pin_bbox, tolerance)) {
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
                // printf("[secondPass]: Pin %s intersects with Pin %s. (Further logic needed)\\n", current_pin.pin_name.c_str(), other_pin_const.pin_name.c_str());
#endif
                // TODO: Add more sophisticated overlap resolution if needed.
                // For now, this just identifies an overlap.
            }
        }

        // Check if pin (with current orientation) extends beyond component's main body outline
        // This requires knowing component's body dimensions (comp.width, comp.height)
        // and assumes pins are defined relative to component.center_x, component.center_y
        // Note: component.width/height are in unrotated component space.
        // Pin coords (current_pin.x_coord, y_coord) are in world space (or board space).
        // We need to transform pin bbox to component's local, unrotated space.

        // Create a temporary pin to test opposite orientation
        if (current_pin.orientation != PinOrientation::Natural) {
            // Construct a temporary Pin by copying members
            Pin test_pin_for_opposite(current_pin.x_coord, current_pin.y_coord, current_pin.pin_name, current_pin.pad_shape, current_pin.getLayerId(), current_pin.getNetId(), current_pin.orientation);
            test_pin_for_opposite.width = current_pin.width;  // copy these as well
            test_pin_for_opposite.height = current_pin.height;
            // Also copy long_side and short_side as getOrientedPinDimensions relies on them
            test_pin_for_opposite.long_side = current_pin.long_side;
            test_pin_for_opposite.short_side = current_pin.short_side;

            PinOrientation opposite_orientation = (current_pin.orientation == PinOrientation::Vertical) ? PinOrientation::Horizontal : PinOrientation::Vertical;
            test_pin_for_opposite.orientation = opposite_orientation;

            double opposite_w = test_pin_for_opposite.width;
            double opposite_h = test_pin_for_opposite.height;
            BLRect opposite_pin_bbox = BLRect(test_pin_for_opposite.x_coord - opposite_w / 2.0, test_pin_for_opposite.y_coord - opposite_h / 2.0, opposite_w, opposite_h);

            bool current_overlaps_any = false;
            bool opposite_overlaps_any = false;

            for (size_t other_idx_check = 0; other_idx_check < component.pins.size(); ++other_idx_check) {
                if (pin_idx == other_idx_check || !component.pins[other_idx_check])
                    continue;
                const Pin& other_pin_const_check = *component.pins[other_idx_check];  // Dereference

                double other_w_check = other_pin_const_check.width;
                double other_h_check = other_pin_const_check.height;
                BLRect other_pin_bbox_check = BLRect(other_pin_const_check.x_coord - other_w_check / 2.0, other_pin_const_check.y_coord - other_h_check / 2.0, other_w_check, other_h_check);

                if (areRectsIntersecting(current_pin_bbox, other_pin_bbox_check, 0.0))  // Stricter check for this test
                    current_overlaps_any = true;
                if (areRectsIntersecting(opposite_pin_bbox, other_pin_bbox_check, 0.0))
                    opposite_overlaps_any = true;
            }

            if (current_overlaps_any && !opposite_overlaps_any) {
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
                printf("[secondPass]: Pin %s: Current orientation (%s) overlaps, opposite (%s) does not. Flipping.\\n",
                       current_pin.pin_name.c_str(),
                       pinOrientationToString(current_pin.orientation),
                       pinOrientationToString(opposite_orientation));
#endif
                current_pin.orientation = opposite_orientation;
                current_pin.SetDimensionsForOrientation();
            } else if (!current_overlaps_any && !opposite_overlaps_any) {
                // If neither overlaps, maybe check which one fits "better" within component boundary,
                // or which one aligns better with the pin's local_edge property if it's not INTERIOR.
                // For now, if current is fine, leave it.
            }
        }
    }
}

void OrientationProcessor::thirdPass_FinalBoundaryCheck(Component& component, double tolerance)
{
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
    printf("[thirdPass]: Final boundary check for component %s\n", component.reference_designator.c_str());
#endif

    // Helper lambdas for string conversion
    auto pinOrientationToString = [](PinOrientation o) {
        if (o == PinOrientation::Vertical)
            return "Vertical";
        if (o == PinOrientation::Horizontal)
            return "Horizontal";
        return "Natural";
    };
    auto localEdgeToString = [](LocalEdge le) {
        if (le == LocalEdge::LEFT)
            return "Left";
        if (le == LocalEdge::RIGHT)
            return "Right";
        if (le == LocalEdge::TOP)
            return "Top";
        if (le == LocalEdge::BOTTOM)
            return "Bottom";
        if (le == LocalEdge::INTERIOR)
            return "Interior";
        return "Unknown";
    };

    // This pass ensures pins respect component boundaries if they are on edges.
    // It assumes component.width and component.height are the unrotated body dimensions.
    // Pin coordinates (pin.x_coord, pin.y_coord) are in global/board space.
    // Component center (component.center_x, component.center_y) is also global/board space.
    // Component rotation (component.rotation) is in degrees.

    // Transform component boundaries to global space or pin to local unrotated component space.
    // Let's transform pin to local unrotated component space.
    double comp_rot_rad_inv = component.rotation * (M_PI / 180.0);

    for (auto& pin_ptr : component.pins)  // Iterate unique_ptr
    {
        if (!pin_ptr)
            continue;
        Pin& pin = *pin_ptr;  // Dereference

        if (pin.local_edge == LocalEdge::INTERIOR || pin.orientation == PinOrientation::Natural)
            continue;  // Focus on edge pins with determined orientations

        // Pin center in world space: (pin.x_coord, pin.y_coord)
        // Pin center relative to component center (world space):
        double rel_pin_x = pin.x_coord - component.center_x;
        double rel_pin_y = pin.y_coord - component.center_y;

        // Rotate this relative vector to align with component's unrotated axes:
        Vec2 local_unrotated_pin_center = rotatePoint(rel_pin_x, rel_pin_y, comp_rot_rad_inv);

        double pin_w = pin.width;
        double pin_h = pin.height;

        // Local unrotated component half-dimensions
        double comp_half_w = component.width / 2.0;
        double comp_half_h = component.height / 2.0;

        bool extends_beyond = false;
        if (pin.orientation == PinOrientation::Horizontal) {                              // Pin is wider than tall in its current orientation
            if (local_unrotated_pin_center.x - pin_w / 2.0 < -comp_half_w - tolerance ||  // Left edge
                local_unrotated_pin_center.x + pin_w / 2.0 > comp_half_w + tolerance ||   // Right edge
                local_unrotated_pin_center.y - pin_h / 2.0 < -comp_half_h - tolerance ||  // Top edge
                local_unrotated_pin_center.y + pin_h / 2.0 > comp_half_h + tolerance)     // Bottom edge
            {
                extends_beyond = true;
            }
        } else {  // Pin is taller than wide (Vertical orientation)
            if (local_unrotated_pin_center.x - pin_w / 2.0 < -comp_half_w - tolerance || local_unrotated_pin_center.x + pin_w / 2.0 > comp_half_w + tolerance ||
                local_unrotated_pin_center.y - pin_h / 2.0 < -comp_half_h - tolerance || local_unrotated_pin_center.y + pin_h / 2.0 > comp_half_h + tolerance) {
                extends_beyond = true;
            }
        }

        if (extends_beyond) {
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
            printf(
                "[thirdPass]: Pin %s: Extends beyond boundaries. On %s. Current orientation: %s\\n", pin.pin_name.c_str(), localEdgeToString(pin.local_edge), pinOrientationToString(pin.orientation));
#endif
            // Try flipping orientation if it's not Natural
            PinOrientation opposite_orientation = (pin.orientation == PinOrientation::Vertical) ? PinOrientation::Horizontal : PinOrientation::Vertical;

            Pin test_pin_opposite(pin.x_coord, pin.y_coord, pin.pin_name, pin.pad_shape, pin.getLayerId(), pin.getNetId(), opposite_orientation);
            test_pin_opposite.width = pin.width;
            test_pin_opposite.height = pin.height;
            // Also copy long_side and short_side as getOrientedPinDimensions relies on them
            test_pin_opposite.long_side = pin.long_side;
            test_pin_opposite.short_side = pin.short_side;

            double opp_pin_w = test_pin_opposite.width;
            double opp_pin_h = test_pin_opposite.height;

            bool opposite_extends_beyond = false;
            if (test_pin_opposite.orientation == PinOrientation::Horizontal) {
                if (local_unrotated_pin_center.x - opp_pin_w / 2.0 < -comp_half_w - tolerance || local_unrotated_pin_center.x + opp_pin_w / 2.0 > comp_half_w + tolerance ||
                    local_unrotated_pin_center.y - opp_pin_h / 2.0 < -comp_half_h - tolerance || local_unrotated_pin_center.y + opp_pin_h / 2.0 > comp_half_h + tolerance) {
                    opposite_extends_beyond = true;
                }
            } else {  // Vertical
                if (local_unrotated_pin_center.x - opp_pin_w / 2.0 < -comp_half_w - tolerance || local_unrotated_pin_center.x + opp_pin_w / 2.0 > comp_half_w + tolerance ||
                    local_unrotated_pin_center.y - opp_pin_h / 2.0 < -comp_half_h - tolerance || local_unrotated_pin_center.y + opp_pin_h / 2.0 > comp_half_h + tolerance) {
                    opposite_extends_beyond = true;
                }
            }

            if (!opposite_extends_beyond) {
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
                printf("  Pin %s: Flipped to %s, which fits within boundaries.\\n", pin.pin_name.c_str(), pinOrientationToString(opposite_orientation));
#endif
                pin.orientation = opposite_orientation;
                pin.SetDimensionsForOrientation();
            } else {
#ifdef ENABLE_ORIENTATION_DEBUG_PRINTF
                printf("  Pin %s: Both current and opposite orientations extend beyond boundaries. No change made.\\n", pin.pin_name.c_str());
#endif
            }
        }
    }
}

void OrientationProcessor::calculatePinBoundingBox(Component& component, double tolerance)
{
    if (component.pins.empty()) {
        component.pin_bbox_min_x = component.pin_bbox_max_x = component.center_x;
        component.pin_bbox_min_y = component.pin_bbox_max_y = component.center_y;
        return;
    }

    component.pin_bbox_min_x = std::numeric_limits<double>::max();
    component.pin_bbox_max_x = std::numeric_limits<double>::lowest();
    component.pin_bbox_min_y = std::numeric_limits<double>::max();
    component.pin_bbox_max_y = std::numeric_limits<double>::lowest();

    for (const auto& pin_ptr : component.pins)  // Iterate unique_ptr
    {
        if (!pin_ptr)
            continue;
        const Pin& pin = *pin_ptr;  // Dereference
        // Consider the pin's own bounding box, not just its center point
        // For simplicity, using pin coordinates + some extent if available, or just coordinates
        // This needs to be more robust by using the pin's actual shape and dimensions
        double half_width = pin.width / 2.0;    // Assuming width is relevant
        double half_height = pin.height / 2.0;  // Assuming height is relevant

        component.pin_bbox_min_x = std::min(component.pin_bbox_min_x, pin.x_coord - half_width);
        component.pin_bbox_max_x = std::max(component.pin_bbox_max_x, pin.x_coord + half_width);
        component.pin_bbox_min_y = std::min(component.pin_bbox_min_y, pin.y_coord - half_height);
        component.pin_bbox_max_y = std::max(component.pin_bbox_max_y, pin.y_coord + half_height);
    }
}

double OrientationProcessor::calculateComponentRotation(const Component& component)
{
    if (component.graphical_elements.empty()) {
        return 0.0;  // No segments to determine rotation
    }

    std::map<int, double> angle_histogram;  // Angle (degrees, rounded) to cumulative length
    const double ANGLE_RESOLUTION = 5.0;    // Degrees

    for (const auto& seg : component.graphical_elements) {
        double dx = seg.end.x - seg.start.x;
        double dy = seg.end.y - seg.start.y;
        double length = std::sqrt(dx * dx + dy * dy);
        if (length < 1e-6)
            continue;  // Skip zero-length segments

        double angle_rad = std::atan2(dy, dx);
        double angle_deg = angle_rad * 180.0 / M_PI;

        // Normalize angle to [0, 180) as orientation is often about an axis
        // A line at 0 deg is same orientation as 180 deg for component body outline
        angle_deg = std::fmod(angle_deg, 180.0);
        if (angle_deg < 0)
            angle_deg += 180.0;

        int rounded_angle_key = static_cast<int>(std::round(angle_deg / ANGLE_RESOLUTION) * ANGLE_RESOLUTION);
        rounded_angle_key = rounded_angle_key % 180;  // Ensure key is within [0, 175]

        angle_histogram[rounded_angle_key] += length;
    }

    if (angle_histogram.empty()) {
        return 0.0;
    }

    // Find the angle with the maximum cumulative length
    double max_length = 0.0;
    int dominant_angle_deg = 0;
    for (const auto& pair : angle_histogram) {
        if (pair.second > max_length) {
            max_length = pair.second;
            dominant_angle_deg = pair.first;
        }
    }
    // Also check the perpendicular angle's popularity (+90 deg)
    // This helps distinguish between a long thin rectangle and a square-ish one with many segments along one axis.
    int perpendicular_angle_key = (dominant_angle_deg + 90) % 180;
    double perpendicular_length = 0.0;
    if (angle_histogram.count(perpendicular_angle_key)) {
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

}  // namespace PCBProcessing

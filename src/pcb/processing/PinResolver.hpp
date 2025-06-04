#pragma once

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "../elements/Component.hpp"
#include "../elements/Pin.hpp"
// ===
// = Check if opposite orientation's extremities are outside of the component's AABB (And within board outlines?)
// ===
class PinResolver
{
public:
    struct PinCollision {
        size_t pin1_idx;
        size_t pin2_idx;
        double overlap_area;
    };

    struct ComponentBounds {
        double min_x, min_y, max_x, max_y;

        ComponentBounds(const Component* comp)
        {
            if (!comp || comp->pins.empty()) {
                min_x = min_y = max_x = max_y = 0.0;
                return;
            }

            // Get bounds from component outline if available, otherwise use pin extents
            BLRect bounds = comp->GetBoundingBox();
            min_x = bounds.x;
            min_y = bounds.y;
            max_x = bounds.x + bounds.w;
            max_y = bounds.y + bounds.h;
        }

        bool contains(double x, double y, double width, double height) const
        {
            double half_w = width / 2.0;
            double half_h = height / 2.0;
            return (x - half_w >= min_x && x + half_w <= max_x && y - half_h >= min_y && y + half_h <= max_y);
        }
    };

    // Main entry point - resolves all pin orientations for a component
    static bool resolveComponentPinOrientations(Component* component)
    {
        if (!component || component->pins.empty())
            return false;

        ComponentBounds bounds(component);
        bool overall_resolved = false;
        // Max iterations to prevent potential infinite loops if logic doesn't converge.
        // This can happen if rotations oscillate (fix one, break another, then reverse).
        int max_iterations = component->pins.size() * 1;  // Heuristic: allow couple of attempts per pin

        for (int iter = 0; iter < max_iterations; ++iter) {
            std::vector<PinCollision> collisions;
            std::set<size_t> out_of_bounds_pins_indices;  // Store indices

            detectCollisions(component->pins, collisions);
            detectOutOfBounds(component->pins, bounds, out_of_bounds_pins_indices);

            std::set<size_t> problematic_pin_indices;
            for (const auto& collision : collisions) {
                problematic_pin_indices.insert(collision.pin1_idx);
                problematic_pin_indices.insert(collision.pin2_idx);
            }
            for (size_t oob_pin_idx : out_of_bounds_pins_indices) {
                problematic_pin_indices.insert(oob_pin_idx);
            }

            if (problematic_pin_indices.empty()) {
                overall_resolved = true;
                break;  // All issues resolved
            }

            std::vector<size_t> pins_to_try_rotating(problematic_pin_indices.begin(), problematic_pin_indices.end());
            // Sort by potential impact (larger pins first) - current heuristic
            std::sort(pins_to_try_rotating.begin(), pins_to_try_rotating.end(), [&component](size_t a, size_t b) {
                // Ensure indices are valid before accessing pins
                if (a >= component->pins.size() || b >= component->pins.size())
                    return false;  // Should not happen
                auto [wa, ha] = component->pins[a]->GetDimensions();
                auto [wb, hb] = component->pins[b]->GetDimensions();
                return (wa * ha) > (wb * hb);
            });

            bool rotation_made_in_this_iteration = false;
            for (size_t pin_idx_to_rotate : pins_to_try_rotating) {
                // Pass component directly to tryRotatePin, it can access component->pins
                if (tryRotatePin(component, pin_idx_to_rotate, bounds)) {
                    rotation_made_in_this_iteration = true;
                    // After a successful rotation, we should re-evaluate everything.
                    // Breaking here will cause the outer loop to re-detect problems and re-sort.
                    break;
                }
            }

            if (!rotation_made_in_this_iteration) {
                // If no rotation in this iteration helped, and there are still problematic pins,
                // further iterations with the same logic are unlikely to resolve them.
                overall_resolved = problematic_pin_indices.empty();
                break;
            }
        }
        return overall_resolved;
    }

private:
    // Check if two axis-aligned rectangles overlap
    static bool rectanglesOverlap(double x1, double y1, double w1, double h1, double x2, double y2, double w2, double h2, double tolerance = 1e-6)
    {
        double half_w1 = w1 / 2.0, half_h1 = h1 / 2.0;
        double half_w2 = w2 / 2.0, half_h2 = h2 / 2.0;

        return !(x1 + half_w1 + tolerance < x2 - half_w2 || x2 + half_w2 + tolerance < x1 - half_w1 || y1 + half_h1 + tolerance < y2 - half_h2 || y2 + half_h2 + tolerance < y1 - half_h1);
    }

    // Calculate overlap area between two rectangles
    static double calculateOverlapArea(double x1, double y1, double w1, double h1, double x2, double y2, double w2, double h2)
    {
        double half_w1 = w1 / 2.0, half_h1 = h1 / 2.0;
        double half_w2 = w2 / 2.0, half_h2 = h2 / 2.0;

        double left = std::max(x1 - half_w1, x2 - half_w2);
        double right = std::min(x1 + half_w1, x2 + half_w2);
        double top = std::max(y1 - half_h1, y2 - half_h2);
        double bottom = std::min(y1 + half_h1, y2 + half_h2);

        if (left >= right || top >= bottom)
            return 0.0;
        return (right - left) * (bottom - top);
    }

    // Detect all pin collisions in component
    static void detectCollisions(const std::vector<std::unique_ptr<Pin>>& pins, std::vector<PinCollision>& collisions)
    {
        collisions.clear();

        for (size_t i = 0; i < pins.size(); ++i) {
            for (size_t j = i + 1; j < pins.size(); ++j) {
                const Pin& pin1 = *pins[i];
                const Pin& pin2 = *pins[j];

                // Skip if pins are on different layers
                if (pin1.GetLayerId() != pin2.GetLayerId())
                    continue;

                auto [w1, h1] = pin1.GetDimensions();
                auto [w2, h2] = pin2.GetDimensions();

                if (rectanglesOverlap(pin1.coords.x_ax, pin1.coords.y_ax, w1, h1, pin2.coords.x_ax, pin2.coords.y_ax, w2, h2)) {
                    double overlap = calculateOverlapArea(pin1.coords.x_ax, pin1.coords.y_ax, w1, h1, pin2.coords.x_ax, pin2.coords.y_ax, w2, h2);
                    if (overlap > 1e-6) {
                        collisions.push_back({i, j, overlap});
                    }
                }
            }
        }
    }

    // Detect pins that extend outside component bounds
    static void detectOutOfBounds(const std::vector<std::unique_ptr<Pin>>& pins, const ComponentBounds& bounds,
                                  std::set<size_t>& oob_pin_indices)  // Changed to oob_pin_indices
    {
        oob_pin_indices.clear();

        for (size_t i = 0; i < pins.size(); ++i) {
            const Pin& pin = *pins[i];
            auto [w, h] = pin.GetDimensions();

            if (!bounds.contains(pin.coords.x_ax, pin.coords.y_ax, w, h)) {
                oob_pin_indices.insert(i);  // Store index
            }
        }
    }

    // Try rotating a specific pin to resolve conflicts
    static bool tryRotatePin(Component* component, size_t pin_idx_to_rotate, const ComponentBounds& bounds)
    {
        if (pin_idx_to_rotate >= component->pins.size())
            return false;

        Pin& pin = *component->pins[pin_idx_to_rotate];
        auto [orig_w, orig_h] = pin.GetDimensions();

        // Skip if pin is square (rotation won't change its AABB significantly for this check)
        // or if pin is not rotatable (e.g. certain pad shapes if we add that logic)
        if (std::abs(orig_w - orig_h) < 1e-6)
            return false;

        // Store original state
        PadShape original_shape = pin.pad_shape;
        PinOrientation original_orientation = pin.orientation;
        // Cache original dimensions as they are stored in the PadShape variant
        double cached_original_width = orig_w;
        double cached_original_height = orig_h;

        // --- Assess current state of the pin_idx_to_rotate ---
        bool was_oob_before = false;
        bool was_colliding_before = false;

        // Check OOB before rotation
        if (!bounds.contains(pin.coords.x_ax, pin.coords.y_ax, cached_original_width, cached_original_height)) {
            printf("Pin %zu is out of bounds before rotation\n", pin_idx_to_rotate);
            was_oob_before = true;
        }

        // Check collisions before rotation for this specific pin
        for (size_t i = 0; i < component->pins.size(); ++i) {
            if (i == pin_idx_to_rotate)
                continue;
            const Pin& other_pin = *component->pins[i];
            if (other_pin.GetLayerId() != pin.GetLayerId())
                continue;

            auto [other_w, other_h] = other_pin.GetDimensions();
            if (rectanglesOverlap(pin.coords.x_ax, pin.coords.y_ax, cached_original_width, cached_original_height, other_pin.coords.x_ax, other_pin.coords.y_ax, other_w, other_h)) {
                printf("Pin %zu is colliding with pin %zu before rotation\n", pin_idx_to_rotate, i);
                was_colliding_before = true;
                break;
            }
        }

        // If the pin is not problematic (neither OOB nor colliding with anything),
        // we generally shouldn't rotate it unless we have a global scoring system.
        // This function is typically called for problematic pins.
        if (!was_oob_before && !was_colliding_before) {
            // It can happen if another pin's rotation made this one non-problematic
            // but it was still in the pins_to_try_rotating list from the start of the iteration.
            // Or, if a pin is part of a collision pair, but isn't itself OOB.
            // For now, proceed to see if rotation makes it *even better* or maintains non-problematic state.
        }

        // --- Perform Rotation ---
        swapPinDimensions(pin);  // This updates pin.width, pin.height internally via getDimensions()
        // --- End Rotation ---

        // --- Check new state of the pin_idx_to_rotate ---
        bool is_now_oob = false;
        bool is_now_colliding = false;
        auto [rotated_w, rotated_h] = pin.GetDimensions();  // Get dimensions after swap

        // 1. Check Out-of-Bounds for the rotated pin
        if (!bounds.contains(pin.coords.x_ax, pin.coords.y_ax, rotated_w, rotated_h)) {
            printf("Pin %zu will be out of bounds after rotation\n", pin_idx_to_rotate);
            is_now_oob = true;
        }

        // 2. Check Collisions for the rotated pin with all other pins
        // Make sure not to cause new collisions or worsen existing ones for *this* pin.
        for (size_t i = 0; i < component->pins.size(); ++i) {
            if (i == pin_idx_to_rotate)
                continue;
            const Pin& other_pin = *component->pins[i];
            if (other_pin.GetLayerId() != pin.GetLayerId())
                continue;

            auto [other_w, other_h] = other_pin.GetDimensions();
            if (rectanglesOverlap(pin.coords.x_ax, pin.coords.y_ax, rotated_w, rotated_h, other_pin.coords.x_ax, other_pin.coords.y_ax, other_w, other_h)) {
                printf("Pin %zu will be colliding with pin %zu after rotation\n", pin_idx_to_rotate, i);
                is_now_colliding = true;
                break;
            }
        }

        // --- Decision Logic ---
        // A rotation is considered beneficial if it resolves an issue (OOB or collision)
        // for the current pin without introducing a new issue for this same pin.
        bool accept_rotation = false;

        if (was_oob_before && !is_now_oob && !is_now_colliding) {  // Fixed OOB, and no new collision for this pin
            printf("Pin %zu was out of bounds and colliding before rotation and is now in bounds and not colliding after rotation\n", pin_idx_to_rotate);
            accept_rotation = true;
        } else if (was_colliding_before && !is_now_colliding && !is_now_oob) {  // Fixed collision, and no new OOB for this pin
            printf("Pin %zu was colliding before rotation and is now not out of bounds and not colliding after rotation\n", pin_idx_to_rotate);
            accept_rotation = true;
        } else if (was_oob_before && was_colliding_before && !is_now_oob && !is_now_colliding) {  // Fixed both
            printf("Pin %zu was out of bounds and colliding before rotation and is now in bounds and not colliding after rotation\n", pin_idx_to_rotate);
            accept_rotation = true;
        } else if (!was_oob_before && !was_colliding_before && !is_now_oob && !is_now_colliding) {
            // Pin was fine, and is still fine after rotation. This path might be taken if a pin
            // was part of an earlier collision pair but the other pin was moved/rotated first.
            // Or if it was simply not square and got selected.
            // Allowing this rotation is fine, as it doesn't worsen the state.
            // Could be debated if this "exploratory" rotation is needed if pin is not problematic.
            // For now, let's say if it doesn't hurt, it's okay.
            printf("Pin %zu was not out of bounds and not colliding before rotation and is now not out of bounds and not colliding after rotation\n", pin_idx_to_rotate);
            accept_rotation = true;
        } else if (was_oob_before && !is_now_oob && is_now_colliding) {
            printf("Pin %zu was out of bounds before rotation and is now in bounds but colliding after rotation\n", pin_idx_to_rotate);
            // This is a special case where we allow a rotation that fixes OOB but introduces a new collision.
            // This tells us that this pin is correctly oriented, but the other pin is not.
            // We should not rotate this pin, but we should rotate the other pin.
            accept_rotation = true;
        } else if (was_colliding_before && !is_now_colliding && is_now_oob) {
            printf("Pin %zu was colliding before rotation and is now out of bounds after rotation\n", pin_idx_to_rotate);
            // This is means we have changed the orientation of the wrong pin.
            // We should not accept this rotation. and instead rotate the other pin.
            accept_rotation = false;
        } else if (was_oob_before && !is_now_oob && is_now_colliding) {
            printf("Pin %zu was out of bounds before rotation and is now colliding after rotation\n", pin_idx_to_rotate);
            // This is means we have changed the orientation of the wrong pin.
            // We should not accept this rotation. and instead rotate the other pin.
            accept_rotation = false;
        }
        // More complex: if it fixed one problem but introduced another (e.g. fixed OOB but now collides).
        // For now, we prefer rotations that lead to a strictly non-problematic state for the rotated pin.

        if (!accept_rotation) {
            // Rotation was not beneficial or made things worse for this pin. Restore.
            pin.pad_shape = original_shape;  // Restores original dimensions within the variant
            pin.orientation = original_orientation;
            printf("Pin %zu was not accepted and is now in original state\n", pin_idx_to_rotate);
            // Explicitly update cached width/height members if swapPinDimensions modified them directly
            // and they are not solely derived from pad_shape on demand.
            // The current swapPinDimensions updates pin.width/height/long_side/short_side.
            std::tie(pin.width, pin.height) = pin.GetDimensions();  // Re-cache from restored shape
            pin.long_side = std::max(pin.width, pin.height);
            pin.short_side = std::min(pin.width, pin.height);
            return false;
        }

        // Rotation accepted for this pin
        return true;
    }

    // Swap pin dimensions by updating its PadShape
    static void swapPinDimensions(Pin& pin)
    {
        std::visit(
            [](auto& shape) {
                using T = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<T, RectanglePad>) {
                    std::swap(shape.width, shape.height);
                } else if constexpr (std::is_same_v<T, CapsulePad>) {
                    std::swap(shape.width, shape.height);
                }
                // CirclePad doesn't need swapping
            },
            pin.pad_shape);

        // Update orientation
        if (pin.orientation == PinOrientation::kHorizontal) {
            pin.orientation = PinOrientation::kVertical;
        } else if (pin.orientation == PinOrientation::kVertical) {
            pin.orientation = PinOrientation::kHorizontal;
        }

        // Update cached dimensions
        std::tie(pin.width, pin.height) = pin.GetDimensions();
        pin.long_side = std::max(pin.width, pin.height);
        pin.short_side = std::min(pin.width, pin.height);
    }
};

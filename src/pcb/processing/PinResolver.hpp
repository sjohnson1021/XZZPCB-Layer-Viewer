#pragma once

#include "../elements/Pin.hpp"
#include "../elements/Component.hpp"
#include <vector>
#include <set>
#include <cmath>
#include <algorithm>

class PinResolver
{
public:
    struct PinCollision
    {
        size_t pin1_idx;
        size_t pin2_idx;
        double overlap_area;
    };

    struct ComponentBounds
    {
        double min_x, min_y, max_x, max_y;

        ComponentBounds(const Component *comp)
        {
            if (!comp || comp->pins.empty())
            {
                min_x = min_y = max_x = max_y = 0.0;
                return;
            }

            // Get bounds from component outline if available, otherwise use pin extents
            BLRect bounds = comp->getBoundingBox();
            min_x = bounds.x;
            min_y = bounds.y;
            max_x = bounds.x + bounds.w;
            max_y = bounds.y + bounds.h;
        }

        bool contains(double x, double y, double width, double height) const
        {
            double half_w = width / 2.0;
            double half_h = height / 2.0;
            return (x - half_w >= min_x && x + half_w <= max_x &&
                    y - half_h >= min_y && y + half_h <= max_y);
        }
    };

    // Main entry point - resolves all pin orientations for a component
    static bool resolveComponentPinOrientations(Component *component)
    {
        if (!component || component->pins.empty())
            return false;

        ComponentBounds bounds(component);
        std::vector<PinCollision> collisions;
        std::set<size_t> problematic_pins;

        // Initial collision detection
        detectCollisions(component->pins, collisions);
        detectOutOfBounds(component->pins, bounds, problematic_pins);

        // Collect all problematic pins
        for (const auto &collision : collisions)
        {
            problematic_pins.insert(collision.pin1_idx);
            problematic_pins.insert(collision.pin2_idx);
        }

        if (problematic_pins.empty())
            return true; // Already good

        // Try to resolve by rotating problematic pins
        bool resolved = false;
        std::vector<size_t> pins_to_rotate(problematic_pins.begin(), problematic_pins.end());

        // Sort by potential impact (larger pins first)
        std::sort(pins_to_rotate.begin(), pins_to_rotate.end(),
                  [&component](size_t a, size_t b)
                  {
                      auto [wa, ha] = component->pins[a]->getDimensions();
                      auto [wb, hb] = component->pins[b]->getDimensions();
                      return (wa * ha) > (wb * hb);
                  });

        // Try rotating pins one by one
        for (size_t pin_idx : pins_to_rotate)
        {
            if (tryRotatePin(component, pin_idx, bounds))
            {
                resolved = true;
            }
        }

        return resolved;
    }

private:
    // Check if two axis-aligned rectangles overlap
    static bool rectanglesOverlap(double x1, double y1, double w1, double h1,
                                  double x2, double y2, double w2, double h2,
                                  double tolerance = 1e-6)
    {
        double half_w1 = w1 / 2.0, half_h1 = h1 / 2.0;
        double half_w2 = w2 / 2.0, half_h2 = h2 / 2.0;

        return !(x1 + half_w1 + tolerance < x2 - half_w2 ||
                 x2 + half_w2 + tolerance < x1 - half_w1 ||
                 y1 + half_h1 + tolerance < y2 - half_h2 ||
                 y2 + half_h2 + tolerance < y1 - half_h1);
    }

    // Calculate overlap area between two rectangles
    static double calculateOverlapArea(double x1, double y1, double w1, double h1,
                                       double x2, double y2, double w2, double h2)
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
    static void detectCollisions(const std::vector<std::unique_ptr<Pin>> &pins, std::vector<PinCollision> &collisions)
    {
        collisions.clear();

        for (size_t i = 0; i < pins.size(); ++i)
        {
            for (size_t j = i + 1; j < pins.size(); ++j)
            {
                const Pin &pin1 = *pins[i];
                const Pin &pin2 = *pins[j];

                // Skip if pins are on different layers
                if (pin1.getLayerId() != pin2.getLayerId())
                    continue;

                auto [w1, h1] = pin1.getDimensions();
                auto [w2, h2] = pin2.getDimensions();

                if (rectanglesOverlap(pin1.x_coord, pin1.y_coord, w1, h1,
                                      pin2.x_coord, pin2.y_coord, w2, h2))
                {
                    double overlap = calculateOverlapArea(pin1.x_coord, pin1.y_coord, w1, h1,
                                                          pin2.x_coord, pin2.y_coord, w2, h2);
                    if (overlap > 1e-6)
                    {
                        collisions.push_back({i, j, overlap});
                    }
                }
            }
        }
    }

    // Detect pins that extend outside component bounds
    static void detectOutOfBounds(const std::vector<std::unique_ptr<Pin>> &pins, const ComponentBounds &bounds,
                                  std::set<size_t> &oob_pins)
    {
        oob_pins.clear();

        for (size_t i = 0; i < pins.size(); ++i)
        {
            const Pin &pin = *pins[i];
            auto [w, h] = pin.getDimensions();

            if (!bounds.contains(pin.x_coord, pin.y_coord, w, h))
            {
                oob_pins.insert(i);
            }
        }
    }

    // Try rotating a specific pin to resolve conflicts
    static bool tryRotatePin(Component *component, size_t pin_idx, const ComponentBounds &bounds)
    {
        if (pin_idx >= component->pins.size())
            return false;

        Pin &pin = *component->pins[pin_idx];
        auto [orig_w, orig_h] = pin.getDimensions();

        // Skip if pin is square (rotation won't help)
        if (std::abs(orig_w - orig_h) < 1e-6)
            return false;

        // Store original state
        PadShape original_shape = pin.pad_shape;
        PinOrientation original_orientation = pin.orientation;

        // Try swapping dimensions
        swapPinDimensions(pin);

        // Check if this rotation resolves conflicts
        bool improved = true;

        // Check collisions with other pins
        for (size_t i = 0; i < component->pins.size(); ++i)
        {
            if (i == pin_idx)
                continue;

            if (component->pins[i]->getLayerId() != pin.getLayerId())
                continue;

            auto [w1, h1] = pin.getDimensions();
            auto [w2, h2] = component->pins[i]->getDimensions();

            if (rectanglesOverlap(pin.x_coord, pin.y_coord, w1, h1,
                                  component->pins[i]->x_coord, component->pins[i]->y_coord, w2, h2))
            {
                improved = false;
                break;
            }
        }

        // Check bounds
        if (improved)
        {
            auto [w, h] = pin.getDimensions();
            if (!bounds.contains(pin.x_coord, pin.y_coord, w, h))
            {
                improved = false;
            }
        }

        if (!improved)
        {
            // Restore original state
            pin.pad_shape = original_shape;
            pin.orientation = original_orientation;
            return false;
        }

        return true;
    }

    // Swap pin dimensions by updating its PadShape
    static void swapPinDimensions(Pin &pin)
    {
        std::visit([](auto &shape)
                   {
                       using T = std::decay_t<decltype(shape)>;
                       if constexpr (std::is_same_v<T, RectanglePad>)
                       {
                           std::swap(shape.width, shape.height);
                       }
                       else if constexpr (std::is_same_v<T, CapsulePad>)
                       {
                           std::swap(shape.width, shape.height);
                       }
                       // CirclePad doesn't need swapping
                   },
                   pin.pad_shape);

        // Update orientation
        if (pin.orientation == PinOrientation::Horizontal)
        {
            pin.orientation = PinOrientation::Vertical;
        }
        else if (pin.orientation == PinOrientation::Vertical)
        {
            pin.orientation = PinOrientation::Horizontal;
        }

        // Update cached dimensions
        std::tie(pin.width, pin.height) = pin.getDimensions();
        pin.long_side = std::max(pin.width, pin.height);
        pin.short_side = std::min(pin.width, pin.height);
    }
};
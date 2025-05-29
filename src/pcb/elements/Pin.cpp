#include "pcb/elements/Pin.hpp"
#include "pcb/elements/Component.hpp" // For parentComponent context
#include "utils/GeometryUtils.hpp"    // For GeometryUtils functions
#include <blend2d.h>
#include <sstream>
#include <cmath>     // For M_PI, cos, sin, abs, sqrt
#include <variant>   // For std::visit
#include <algorithm> // For std::max, std::min if needed for specific shape calcs

// Define M_PI if not already available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper to get world coordinates of pin center and its world rotation
std::pair<Vec2, double> GetPinWorldTransform(const Pin &pin, const Component *parentComponent)
{
    if (!parentComponent)
    {
        // Pin is standalone or context is missing. Treat its coords as world, 0 rotation.
        return {{pin.x_coord, pin.y_coord}, pin.rotation};
    }

    double comp_rot_rad = parentComponent->rotation * (M_PI / 180.0);
    double cos_comp = std::cos(comp_rot_rad);
    double sin_comp = std::sin(comp_rot_rad);

    // Pin's own coordinates (x_coord, y_coord) are relative to component center
    double world_x = parentComponent->center_x + (pin.x_coord * cos_comp - pin.y_coord * sin_comp);
    double world_y = parentComponent->center_y + (pin.x_coord * sin_comp + pin.y_coord * cos_comp);

    double world_rotation_deg = parentComponent->rotation + pin.rotation; // Pin's own rotation is added to component's
    world_rotation_deg = fmod(world_rotation_deg, 360.0);
    if (world_rotation_deg < 0)
        world_rotation_deg += 360.0;

    return {{world_x, world_y}, world_rotation_deg};
}

BLRect Pin::getBoundingBox(const Component *parentComponent) const
{
    auto [world_pin_center, pin_world_rotation_deg] = GetPinWorldTransform(*this, parentComponent);

    double w_pin, h_pin;
    std::tie(w_pin, h_pin) = Pin::getDimensionsFromShape(pad_shape); // Use static helper for base dimensions

    if (w_pin == 0 || h_pin == 0)
    {
        return BLRect(world_pin_center.x, world_pin_center.y, 0, 0);
    }

    double half_w = w_pin / 2.0;
    double half_h = h_pin / 2.0;

    double angle_rad = pin_world_rotation_deg * (M_PI / 180.0);
    double cos_rot = std::cos(angle_rad);
    double sin_rot = std::sin(angle_rad);

    // Coordinates of the 4 corners of the pin's unrotated bounding box, centered at its local origin
    Vec2 local_corners[4] = {
        {-half_w, -half_h}, {half_w, -half_h}, {half_w, half_h}, {-half_w, half_h}};

    double min_x_world = world_pin_center.x, max_x_world = world_pin_center.x;
    double min_y_world = world_pin_center.y, max_y_world = world_pin_center.y;
    bool first = true;

    for (int i = 0; i < 4; ++i)
    {
        // Rotate corner relative to pin's local origin
        double rotated_x = local_corners[i].x * cos_rot - local_corners[i].y * sin_rot;
        double rotated_y = local_corners[i].x * sin_rot + local_corners[i].y * cos_rot;

        // Translate to world pin center
        double corner_world_x = rotated_x + world_pin_center.x;
        double corner_world_y = rotated_y + world_pin_center.y;

        if (first)
        {
            min_x_world = max_x_world = corner_world_x;
            min_y_world = max_y_world = corner_world_y;
            first = false;
        }
        else
        {
            min_x_world = std::min(min_x_world, corner_world_x);
            max_x_world = std::max(max_x_world, corner_world_x);
            min_y_world = std::min(min_y_world, corner_world_y);
            max_y_world = std::max(max_y_world, corner_world_y);
        }
    }
    return BLRect(min_x_world, min_y_world, max_x_world - min_x_world, max_y_world - min_y_world);
}

bool Pin::isHit(const Vec2 &worldMousePos, float tolerance, const Component *parentComponent) const
{
    // Pin coordinates are now global, no need for component transformation
    Vec2 pin_center(x_coord, y_coord);

    // Transform mouse position relative to pin center
    Vec2 mouse_relative_to_pin = {
        worldMousePos.x - pin_center.x,
        worldMousePos.y - pin_center.y};

    // Now, check if mouse_relative_to_pin is within the pin's specific geometry
    return std::visit(
        [&](const auto &shape) -> bool
        {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, CirclePad>)
            {
                return GeometryUtils::isPointInCircle(
                    mouse_relative_to_pin,
                    Vec2(0, 0), // Circle is centered at pin coordinates
                    shape.radius,
                    static_cast<double>(tolerance));
            }
            else if constexpr (std::is_same_v<T, RectanglePad>)
            {
                double half_w = shape.width / 2.0 + tolerance;
                double half_h = shape.height / 2.0 + tolerance;
                // Rectangle is centered at pin coordinates
                return (std::abs(mouse_relative_to_pin.x) <= half_w &&
                        std::abs(mouse_relative_to_pin.y) <= half_h);
            }
            else if constexpr (std::is_same_v<T, CapsulePad>)
            {
                // For CapsulePad, we need to check if the point is near the capsule shape
                // The capsule is centered at pin coordinates
                double radius = shape.height / 2.0;
                double rect_length = shape.width - shape.height;

                if (rect_length <= 0)
                {
                    // If no rectangular part, treat as a circle
                    return GeometryUtils::isPointInCircle(
                        mouse_relative_to_pin,
                        Vec2(0, 0),
                        radius,
                        static_cast<double>(tolerance));
                }

                // Check if point is in the rectangular part
                double half_rect_length = rect_length / 2.0;
                if (std::abs(mouse_relative_to_pin.x) <= half_rect_length)
                {
                    return std::abs(mouse_relative_to_pin.y) <= radius + tolerance;
                }

                // Check if point is in either end cap
                double dx = std::abs(mouse_relative_to_pin.x) - half_rect_length;
                if (dx <= 0)
                    return false; // Already checked in rectangular part

                Vec2 cap_center(
                    mouse_relative_to_pin.x > 0 ? half_rect_length : -half_rect_length,
                    0);
                return GeometryUtils::isPointInCircle(
                    mouse_relative_to_pin,
                    cap_center,
                    radius,
                    static_cast<double>(tolerance));
            }
            return false;
        },
        pad_shape);
}

std::string Pin::getInfo(const Component *parentComponent) const
{
    std::ostringstream oss;
    oss << "Pin: " << pin_name << "\n";
    auto [world_pos, world_rot] = GetPinWorldTransform(*this, parentComponent);

    if (parentComponent)
    {
        oss << "Component: " << parentComponent->reference_designator << "\n";
        oss << "Local Pin Anchor: (" << x_coord << ", " << y_coord << ") Rot: " << rotation << " deg\n";
    }
    oss << "World Pin Center: (" << world_pos.x << ", " << world_pos.y << ") World Rot: " << world_rot << " deg\n";
    oss << "Layer: " << m_layerId << ", Side: " << side << "\n";
    if (m_netId != -1)
    {
        oss << "Net ID: " << m_netId << "\n";
    }
    oss << "Shape: ";
    std::visit([&oss](const auto &s)
               {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, CirclePad>) oss << "Circle (R=" << s.radius << ")";
        else if constexpr (std::is_same_v<T, RectanglePad>) oss << "Rect (W=" << s.width << ", H=" << s.height << ")";
        else if constexpr (std::is_same_v<T, CapsulePad>) oss << "Capsule (W=" << s.width << ", H=" << s.height << ")"; }, pad_shape);
    oss << "\nOrientation: " << getOrientationName();
    if (!diode_reading.empty())
    {
        oss << "\nDiode: " << diode_reading;
    }
    return oss.str();
}

void Pin::translate(double dx, double dy)
{
    // Pin coordinates (x_coord, y_coord) are relative to the component's center.
    // So, when the *component* is translated by (dx, dy) in world space,
    // its center_x and center_y are updated.
    // The pin's local x_coord, y_coord *relative to the component center* do not change.

    // However, if a Pin element is ever treated as a standalone element being directly
    // translated in the Board's m_elementsByLayer (which is not its typical use case,
    // as pins are owned by components), then this translate method would apply.
    // In that scenario, x_coord and y_coord would be considered world coordinates.

    // Given that Pin::translate is called by Component::translate, and pins are part of components,
    // their x_coord and y_coord (which are component-local offsets) should *not* change here.
    // The component's center_x, center_y are translated, and the pin's world position is derived from that.

    // If a pin were to be a standalone element *not* part of a component, and its
    // x_coord, y_coord were to be interpreted as world coordinates, then this would be:
    // x_coord += dx;
    // y_coord += dy;

    // For the current design where Pins are part of Components and their x_coord, y_coord
    // are local offsets, this method should arguably do nothing, as the parent Component's
    // translation handles the change in world position.
    // However, for consistency with the Element interface, we provide an implementation.
    // If a pin *could* be standalone, this is where its primary point would be moved.
    // Let's assume for normalization purposes that if called directly on a pin (e.g. if it was
    // a standalone element outside a component context, which is unusual), its x_coord/y_coord are world.
    x_coord += dx;
    y_coord += dy;

    // The PadShape offsets (CirclePad::x_offset, etc.) are relative to the Pin's (x_coord, y_coord).
    // These should NOT be translated here, as they define the shape relative to the pin's anchor.
}
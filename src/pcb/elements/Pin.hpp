#pragma once

#include <algorithm>  // For std::max, std::min
#include <cmath>      // For std::min/max in constructor helper
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "pcb/elements/Element.hpp"  // Include base class
#include "utils/Vec2.hpp"            // For Vec2

// Forward declare Component because Pin needs to know about it for context in virtual methods
class Component;

// Define the individual pad shapes
struct CirclePad {
    double radius;
};

struct RectanglePad {  // Can also represent a square
    double width;
    double height;
    // double rotation = 0.0; // Optional: if pads can be rotated
};

struct CapsulePad {  // A rectangle with semicircular ends
    double width;    // Total width including semicircles
    double height;   // Diameter of the semicircular ends / height of the rectangular part
    // double rotation = 0.0; // Optional
};

// The PadGeometry variant can hold any of the defined shapes
using PadShape = std::variant<CirclePad, RectanglePad, CapsulePad>;

// Enum for pin orientation if needed, similar to old XZZPCBLoader.h
enum class PinOrientation : std::uint8_t {
    kNatural,     // Default
    kHorizontal,  // Long axis horizontal
    kVertical     // Long axis vertical
};
enum class LocalEdge : std::uint8_t {
    kUnknown,
    kTop,
    kBottom,
    kLeft,
    kRight,
    kInterior
};

class Pin : public Element
{  // Inherit from Element
public:
    Pin(Vec2 coords, std::string name, PadShape shape, int layer, int net_id = -1, PinOrientation orientation = PinOrientation::kNatural, int side = 0)
        : Element(layer, ElementType::kPin, net_id),  // Call base constructor
          coords(coords),
          pin_name(std::move(name)),
          pad_shape(shape),
          orientation(orientation),
          long_side(std::max(width, height)),
          short_side(std::min(width, height)),
          side(side)
    {
        // Initialize width, height, long_side, short_side from pad_shape
        std::tie(width, height) = GetDimensionsFromShape(pad_shape);
    }

    // Copy constructor
    Pin(const Pin& other)
        : Element(other.GetLayerId(), other.GetElementType(), other.GetNetId()),  // Initialize Element base
          coords(other.coords),
          pin_name(other.pin_name),
          pad_shape(other.pad_shape),
          local_edge(other.local_edge),
          side(other.side),
          diode_reading(other.diode_reading),
          orientation(other.orientation),
          rotation(other.rotation),
          width(other.width),
          height(other.height),
          long_side(other.long_side),
          short_side(other.short_side),
          debug_color(other.debug_color)
    {
        // Element's m_isGloballyVisible is initialized to true by its constructor.
        // If the copied pin should inherit the source pin's visibility state, set it explicitly.
        SetVisible(other.IsVisible());
    }

    // --- Overridden virtual methods ---
    // These will need the parent Component's transformation context
    BLRect GetBoundingBox(const Component* parent_component = nullptr) const override;
    bool IsHit(const Vec2& world_mouse_pos, float tolerance, const Component* parent_component = nullptr) const override;
    std::string GetInfo(const Component* parent_component = nullptr) const override;
    void Translate(double dist_x, double dist_y) override;
    void Mirror(double center_axis) override;

    // --- Pin-specific Member Data ---
    // get world transform
    static std::pair<Vec2, double> GetPinWorldTransform(const Pin& pin, const Component* parent_component);
    Vec2 coords {};
    std::string pin_name;
    PadShape pad_shape;
    LocalEdge local_edge = LocalEdge::kUnknown;

    int side = 0;  // e.g., 0 for top, 1 for bottom
    std::string diode_reading;
    PinOrientation orientation = PinOrientation::kNatural;
    double rotation = 0.0;  // Degrees, if individual pins can rotate relative to component (unlikely for most uses)

    double width = 0.0;
    double height = 0.0;
    double long_side = 0.0;
    double short_side = 0.0;
    // layer and net_id are in Element

    // --- Pin-specific Getters & Helpers ---
    [[nodiscard]] std::string GetEdgeName() const
    {
        std::string name_str = "UNKNOWN";
        switch (local_edge) {
            case LocalEdge::kTop:
                name_str = "TOP";
                break;
            case LocalEdge::kBottom:
                name_str = "BOTTOM";
                break;
            case LocalEdge::kLeft:
                name_str = "LEFT";
                break;
            case LocalEdge::kRight:
                name_str = "RIGHT";
                break;
            case LocalEdge::kInterior:
                name_str = "INTERIOR";
                break;
            default:
                break;
        }
        return name_str;
    }
    [[nodiscard]] std::string GetOrientationName() const
    {
        switch (orientation) {
            case PinOrientation::kNatural:
                return "Natural";
            case PinOrientation::kHorizontal:
                return "Horizontal";
            case PinOrientation::kVertical:
                return "Vertical";
            default:
                return "Unknown Orientation";
        }
    }

    static std::pair<double, double> GetDimensionsFromShape(const PadShape& shape_variant)
    {
        return std::visit(
            [](const auto& g) -> std::pair<double, double> {
                using T = std::decay_t<decltype(g)>;
                if constexpr (std::is_same_v<T, CirclePad>) {
                    return {g.radius * 2, g.radius * 2};
                } else if constexpr (std::is_same_v<T, RectanglePad>) {
                    return {g.width, g.height};
                } else if constexpr (std::is_same_v<T, CapsulePad>) {
                    return {g.width, g.height};
                }
                return {0.0, 0.0};
            },
            shape_variant);
    }

    [[nodiscard]] double GetRadius() const
    {
        return std::visit(
            [](const auto& g) -> double {
                using T = std::decay_t<decltype(g)>;
                if constexpr (std::is_same_v<T, CirclePad>) {
                    return g.radius;
                } else if constexpr (std::is_same_v<T, RectanglePad>) {
                    return std::min(g.width, g.height) / 2.0;
                } else if constexpr (std::is_same_v<T, CapsulePad>) {
                    return std::min(g.width, g.height) / 2.0;
                }
                return 0.5;  // Default fallback
            },
            pad_shape);
    }

    [[nodiscard]] std::pair<double, double> GetDimensions() const
    {
        return std::visit(
            [](const auto& shape) -> std::pair<double, double> {
                using T = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<T, CirclePad>) {
                    return {shape.radius * 2, shape.radius * 2};
                } else if constexpr (std::is_same_v<T, RectanglePad>) {
                    return {shape.width, shape.height};
                } else if constexpr (std::is_same_v<T, CapsulePad>) {
                    return {shape.width, shape.height};
                }
                return {1.0, 1.0};  // Default fallback
            },
            pad_shape);
    }

    [[nodiscard]] bool IsRounded() const
    {
        return std::visit(
            [](const auto& g) -> bool {
                using T = std::decay_t<decltype(g)>;
                return std::is_same_v<T, CirclePad> || std::is_same_v<T, CapsulePad>;
            },
            pad_shape);
    }

    BLRgba32 debug_color = BLRgba32(0, 0, 0, 0);

    void SetDimensionsForOrientation()
    {
        // Ensure long_side is width, short_side is height
        long_side = std::max(width, height);
        short_side = std::min(width, height);
        if (orientation == PinOrientation::kHorizontal) {
            if (std::holds_alternative<RectanglePad>(pad_shape)) {
                pad_shape = RectanglePad {long_side, short_side};
            } else if (std::holds_alternative<CapsulePad>(pad_shape)) {
                pad_shape = CapsulePad {long_side, short_side};
            }
        } else if (orientation == PinOrientation::kVertical) {
            if (std::holds_alternative<RectanglePad>(pad_shape)) {
                pad_shape = RectanglePad {short_side, long_side};
            } else if (std::holds_alternative<CapsulePad>(pad_shape)) {
                pad_shape = CapsulePad {short_side, long_side};
            }
        } else {
            // Natural: use pad_shape
            std::tie(width, height) = GetDimensionsFromShape(pad_shape);
            long_side = std::max(width, height);
            short_side = std::min(width, height);
        }
    }
};

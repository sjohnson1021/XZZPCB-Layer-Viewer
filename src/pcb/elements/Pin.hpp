#pragma once

#include "pcb/elements/Element.hpp" // Include base class
#include "utils/Vec2.hpp"           // For Vec2
#include <string>
#include <variant>
#include <vector>
#include <cmath>     // For std::min/max in constructor helper
#include <algorithm> // For std::max, std::min

// Forward declare Component because Pin needs to know about it for context in virtual methods
class Component;

// Define the individual pad shapes
struct CirclePad
{
    double radius;
};

struct RectanglePad
{ // Can also represent a square
    double width;
    double height;
    // double rotation = 0.0; // Optional: if pads can be rotated
};

struct CapsulePad
{                  // A rectangle with semicircular ends
    double width;  // Total width including semicircles
    double height; // Diameter of the semicircular ends / height of the rectangular part
    // double rotation = 0.0; // Optional
};

// The PadGeometry variant can hold any of the defined shapes
using PadShape = std::variant<CirclePad, RectanglePad, CapsulePad>;

// Enum for pin orientation if needed, similar to old XZZPCBLoader.h
enum class PinOrientation
{
    Natural,    // Default
    Horizontal, // Long axis horizontal
    Vertical    // Long axis vertical
};
enum class LocalEdge
{
    UNKNOWN,
    TOP,
    BOTTOM,
    LEFT,
    RIGHT,
    INTERIOR
};

class Pin : public Element
{ // Inherit from Element
public:
    Pin(double x, double y, const std::string &name, PadShape shape,
        int layer, int net_id = -1, PinOrientation orientation = PinOrientation::Natural, int side = 0)
        : Element(layer, ElementType::PIN, net_id), // Call base constructor
          x_coord(x), y_coord(y), pin_name(name), pad_shape(shape),
          orientation(orientation), side(side)
    {
        // Initialize initial_width, initial_height, long_side, short_side from pad_shape
        std::tie(initial_width, initial_height) = getDimensionsFromShape(pad_shape);
        long_side = std::max(initial_width, initial_height);
        short_side = std::min(initial_width, initial_height);
    }

    // Copy constructor
    Pin(const Pin &other)
        : Element(other.m_layerId, other.m_type, other.m_netId), // Initialize Element base
          x_coord(other.x_coord),
          y_coord(other.y_coord),
          pin_name(other.pin_name),
          pad_shape(other.pad_shape),
          local_edge(other.local_edge),
          side(other.side),
          diode_reading(other.diode_reading),
          orientation(other.orientation),
          rotation(other.rotation),
          initial_width(other.initial_width),
          initial_height(other.initial_height),
          long_side(other.long_side),
          short_side(other.short_side),
          debug_color(other.debug_color)
    {
        // Element's m_isGloballyVisible is initialized to true by its constructor.
        // If the copied pin should inherit the source pin's visibility state, set it explicitly.
        setVisible(other.isVisible());
    }

    // --- Overridden virtual methods ---
    // These will need the parent Component's transformation context
    BLRect getBoundingBox(const Component *parentComponent) const override;
    bool isHit(const Vec2 &worldMousePos, float tolerance, const Component *parentComponent) const override;
    std::string getInfo(const Component *parentComponent) const override;
    void translate(double dx, double dy) override;

    // --- Pin-specific Member Data ---
    double x_coord; // Local to component center
    double y_coord; // Local to component center
    std::string pin_name;
    PadShape pad_shape;
    LocalEdge local_edge = LocalEdge::UNKNOWN;

    int side = 0; // e.g., 0 for top, 1 for bottom
    std::string diode_reading;
    PinOrientation orientation = PinOrientation::Natural;
    double rotation = 0.0; // Degrees, if individual pins can rotate relative to component (unlikely for most uses)

    double initial_width = 0.0;
    double initial_height = 0.0;
    double long_side = 0.0;
    double short_side = 0.0;
    // layer and net_id are in Element

    // --- Pin-specific Getters & Helpers ---
    std::string getEdgeName() const
    {
        std::string name_str = "UNKNOWN";
        switch (local_edge)
        {
        case LocalEdge::TOP:
            name_str = "TOP";
            break;
        case LocalEdge::BOTTOM:
            name_str = "BOTTOM";
            break;
        case LocalEdge::LEFT:
            name_str = "LEFT";
            break;
        case LocalEdge::RIGHT:
            name_str = "RIGHT";
            break;
        case LocalEdge::INTERIOR:
            name_str = "INTERIOR";
            break;
        default:
            break;
        }
        return name_str;
    }
    std::string getOrientationName() const
    {
        switch (orientation)
        {
        case PinOrientation::Natural:
            return "Natural";
        case PinOrientation::Horizontal:
            return "Horizontal";
        case PinOrientation::Vertical:
            return "Vertical";
        default:
            return "Unknown Orientation";
        }
    }

    static std::pair<double, double> getDimensionsFromShape(const PadShape &shape_variant)
    {
        return std::visit([](const auto &g) -> std::pair<double, double>
                          {
            using T = std::decay_t<decltype(g)>;
            if constexpr (std::is_same_v<T, CirclePad>) {
                return {g.radius * 2, g.radius * 2};
            } else if constexpr (std::is_same_v<T, RectanglePad>) {
                return {g.width, g.height};
            } else if constexpr (std::is_same_v<T, CapsulePad>) {
                return {g.width, g.height}; 
            }
            return {0.0, 0.0}; }, shape_variant);
    }

    double getRadius() const
    {
        return std::visit([](const auto &g) -> double
                          {
                              using T = std::decay_t<decltype(g)>;
                              if constexpr (std::is_same_v<T, CirclePad>)
                              {
                                  return g.radius;
                              }
                              else if constexpr (std::is_same_v<T, RectanglePad>)
                              {
                                  return std::min(g.width, g.height) / 2.0;
                              }
                              else if constexpr (std::is_same_v<T, CapsulePad>)
                              {
                                  return std::min(g.width, g.height) / 2.0;
                              }
                              return 0.5; // Default fallback
                          },
                          pad_shape);
    }

    std::pair<double, double> getDimensions() const
    {
        return std::visit([](const auto &g) -> std::pair<double, double>
                          {
                              using T = std::decay_t<decltype(g)>;
                              if constexpr (std::is_same_v<T, CirclePad>)
                              {
                                  return {g.radius * 2, g.radius * 2};
                              }
                              else if constexpr (std::is_same_v<T, RectanglePad>)
                              {
                                  return {g.width, g.height};
                              }
                              else if constexpr (std::is_same_v<T, CapsulePad>)
                              {
                                  return {g.width, g.height};
                              }
                              return {1.0, 1.0}; // Default fallback
                          },
                          pad_shape);
    }

    bool isRounded() const
    {
        return std::visit([](const auto &g) -> bool
                          {
            using T = std::decay_t<decltype(g)>;
            return std::is_same_v<T, CirclePad> || std::is_same_v<T, CapsulePad>; }, pad_shape);
    }

    BLRgba32 debug_color = BLRgba32(0, 0, 0, 0);
};
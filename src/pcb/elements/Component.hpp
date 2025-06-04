#pragma once

#include <cmath>  // For std::cos, std::sin
#include <math.h>
#include <memory>   // For std::unique_ptr
#include <sstream>  // For std::stringstream
#include <string>
#include <utility>
#include <vector>

#include <blend2d.h>  // For BLRect

#include "Element.hpp"    // Include the Element base class
#include "Pin.hpp"        // Include the Pin definition
#include "TextLabel.hpp"  // Include the TextLabel definition

#include "utils/Constants.hpp"  // For kPi
#include "utils/Vec2.hpp"       // For Vec2 struct

// Helper struct for line segments (e.g., for silkscreen or courtyard)
struct LineSegment {
    Vec2 start {};  // Changed from Point2D to Vec2
    Vec2 end {};    // Changed from Point2D to Vec2
    double thickness = 0.1;
    int layer = 0;  // If segments can be on different layers relative to component
};

enum class ComponentElementType  // Renamed from ComponentType to avoid conflict if Element has ElementType
{
    kSmd,
    kThroughHole,
    kOther
};
enum class MountingSide {
    kTop,
    kBottom,
    // Both // Consider if 'Both' is valid for a single component instance
};

class Component : public Element  // Now inherits from Element
{
public:
    Component(std::string ref_des, std::string val, double x, double y, int layer = 0, int net_id = -1)
        : Element(layer, ElementType::kComponent, net_id),  // Call base constructor
          reference_designator(std::move(ref_des)),
          value(std::move(val)),
          center_x(x),
          center_y(y)
    {
    }

    // Copy constructor for deep copy
    Component(const Component& other)
        : Element(other.GetLayerId(), other.GetElementType(), other.GetNetId()),  // Initialize Element base
          reference_designator(other.reference_designator),
          value(other.value),
          footprint_name(other.footprint_name),
          center_x(other.center_x),
          center_y(other.center_y),
          rotation(other.rotation),
          width(other.width),
          height(other.height),
          pin_bbox_min_x(other.pin_bbox_min_x),
          pin_bbox_max_x(other.pin_bbox_max_x),
          pin_bbox_min_y(other.pin_bbox_min_y),
          pin_bbox_max_y(other.pin_bbox_max_y),
          is_single_pin(other.is_single_pin),
          is_two_pad(other.is_two_pad),
          is_wide_component(other.is_wide_component),
          is_tall_component(other.is_tall_component),
          is_qfp(other.is_qfp),
          is_connector(other.is_connector),
          left_edge_pin_indices(other.left_edge_pin_indices),
          right_edge_pin_indices(other.right_edge_pin_indices),
          top_edge_pin_indices(other.top_edge_pin_indices),
          bottom_edge_pin_indices(other.bottom_edge_pin_indices),
          layer(other.layer),
          side(other.side),
          type(other.type),
          graphical_elements(other.graphical_elements)
    {
        // Deep copy for std::vector<std::unique_ptr<Pin>>
        pins.reserve(other.pins.size());
        for (const auto& pin_ptr : other.pins) {
            if (pin_ptr) {
                pins.emplace_back(std::make_unique<Pin>(*pin_ptr));  // Uses Pin's copy constructor
            } else {
                pins.emplace_back(nullptr);
            }
        }

        // Deep copy for std::vector<std::unique_ptr<TextLabel>>
        text_labels.reserve(other.text_labels.size());
        for (const auto& label_ptr : other.text_labels) {
            if (label_ptr) {
                text_labels.emplace_back(std::make_unique<TextLabel>(*label_ptr));  // Uses TextLabel's copy constructor
            } else {
                text_labels.emplace_back(nullptr);
            }
        }
    }

    // Member Data
    std::string reference_designator;  // e.g., "R1", "U100"
    std::string value;                 // e.g., "10k", "ATMEGA328P"
    std::string footprint_name;        // e.g., "0805", "TQFP32"

    // Position and Orientation (typically of the component's geometric center)
    // Vec2 center; // Preferring separate center_x, center_y for now to match existing
    double center_x = 0.0;
    double center_y = 0.0;
    double rotation = 0.0;  // Degrees

    // We need to add a method to get the center of the component, which is the center of the min and max x an y coordinates of the graphical elements
    [[nodiscard]] Vec2 GetCenter() const
    {
        double kMinX = std::numeric_limits<double>::max();
        double kMaxX = std::numeric_limits<double>::min();
        double kMinY = std::numeric_limits<double>::max();
        double kMaxY = std::numeric_limits<double>::min();

        for (const auto segment : graphical_elements) {
            kMinX = std::min(kMinX, segment.start.x_ax);
            kMaxX = std::max(kMaxX, segment.start.x_ax);
            kMinY = std::min(kMinY, segment.start.y_ax);
            kMaxY = std::max(kMaxY, segment.start.y_ax);
        }

        return Vec2((kMinX + kMaxX) / 2.0, (kMinY + kMaxY) / 2.0);
    }

    // Component dimensions (as potentially reported by the PCB file or calculated)
    double width = 0.0;   // Width of the component's body/courtyard
    double height = 0.0;  // Height of the component's body/courtyard

    // Calculated bounding box of the component's pins (in component-local, unrotated space initially)
    // These might be better as BLRect or Vec2 min/max
    double pin_bbox_min_x = 0.0;
    double pin_bbox_max_x = 0.0;
    double pin_bbox_min_y = 0.0;
    double pin_bbox_max_y = 0.0;

    // Flags for component characteristics (used in orientation heuristics)
    bool is_single_pin = false;
    bool is_two_pad = false;         // Typically resistors, capacitors
    bool is_wide_component = false;  // Width significantly greater than height
    bool is_tall_component = false;  // Height significantly greater than width
    bool is_qfp = false;             // Quad Flat Package (pins on all four sides)
    bool is_connector = false;       // Often has many pins along one or two edges

    // Indices of pins located on the primary edges of the pin bounding box
    std::vector<size_t> left_edge_pin_indices {};
    std::vector<size_t> right_edge_pin_indices {};
    std::vector<size_t> top_edge_pin_indices {};
    std::vector<size_t> bottom_edge_pin_indices {};

    int layer = 0;  // Primary layer the component resides on
    MountingSide side = MountingSide::kTop;
    ComponentElementType type = ComponentElementType::kSmd;  // Renamed enum type

    std::vector<std::unique_ptr<Pin>> pins {};               // New
    std::vector<std::unique_ptr<TextLabel>> text_labels {};  // New
    std::vector<LineSegment> graphical_elements {};          // For silkscreen, courtyard, assembly drawings, etc.

    // --- Virtual methods similar to Element subclasses (for polymorphism if Components become part of Element hierarchy later) ---
    // Or, these could be non-virtual if Component is always a top-level distinct type.
    // For now, let's assume they might be part of a common interface for inspectable/hittable objects.
    virtual BLRect GetBoundingBox(const Component* parentComponent = nullptr) const override
    {
        // For a component, we don't need the parent component parameter since it is the parent
        double const kCompW = width > 0 ? width : 0.1;
        double const kCompH = height > 0 ? height : 0.1;
        Vec2 center = GetCenter();
        double comp_cx = center.x_ax;
        double comp_cy = center.y_ax;
        double comp_rot_rad;
        comp_rot_rad = rotation * (kPi / 180.0);
        double cos_r = std::cos(comp_rot_rad);
        double sin_r = std::sin(comp_rot_rad);

        // Local corners (relative to component's local origin 0,0 before rotation/translation)
        BLPoint local_corners[4] = {{-kCompW / 2.0, -kCompH / 2.0}, {kCompW / 2.0, -kCompH / 2.0}, {kCompW / 2.0, kCompH / 2.0}, {-kCompW / 2.0, kCompH / 2.0}};

        BLPoint world_corners[4];
        for (int i = 0; i < 4; ++i) {
            // Rotate
            double rx = (local_corners[i].x * cos_r) - (local_corners[i].y * sin_r);
            double ry = (local_corners[i].x * sin_r) + (local_corners[i].y * cos_r);
            // Translate
            world_corners[i].x = rx + comp_cx;
            world_corners[i].y = ry + comp_cy;
        }

        // Find min/max of world_corners to form AABB
        double min_wx = world_corners[0].x;
        double max_wx = world_corners[0].x;
        double min_wy = world_corners[0].y;
        double max_wy = world_corners[0].y;
        for (int i = 1; i < 4; ++i) {
            min_wx = std::min(min_wx, world_corners[i].x);
            max_wx = std::max(max_wx, world_corners[i].x);
            min_wy = std::min(min_wy, world_corners[i].y);
            max_wy = std::max(max_wy, world_corners[i].y);
        }
        return BLRect(min_wx, min_wy, max_wx - min_wx, max_wy - min_wy);
    }

    bool IsHit(const Vec2& world_mouse, float tolerance, const Component* parentComponent = nullptr) const override
    {
        // For a component, we don't need the parent component parameter since it is the parent
        BLRect bounds = GetBoundingBox();

        // First check if point is within the bounding box (with tolerance)
        if (world_mouse.x_ax < bounds.x - tolerance || world_mouse.x_ax > bounds.x + bounds.w + tolerance || world_mouse.y_ax < bounds.y - tolerance ||
            world_mouse.y_ax > bounds.y + bounds.h + tolerance) {
            return false;
        }

        // Transform mouse position to component's local space
        double const kCompCx = center_x;
        double const kCompCy = center_y;
        double comp_rot_rad = NAN;
        comp_rot_rad = -rotation * (kPi / 180.0);  // Negative rotation to transform back
        double const kCosR = std::cos(comp_rot_rad);
        double const kSinR = std::sin(comp_rot_rad);

        // Translate to origin, rotate, then translate back
        double local_x = NAN;
        local_x = world_mouse.x_ax - kCompCx;
        double local_y = NAN;
        local_y = world_mouse.y_ax - kCompCy;
        double const kRotatedX = (local_x * kCosR) - (local_y * kSinR);
        double const kRotatedY = (local_x * kSinR) + (local_y * kCosR);

        // Check if point is within the component's rectangle in local space
        double const kHalfWidth = width / 2.0;
        double const kHalfHeight = height / 2.0;
        return kRotatedX >= -kHalfWidth - tolerance && kRotatedX <= kHalfWidth + tolerance && kRotatedY >= -kHalfHeight - tolerance && kRotatedY <= kHalfHeight + tolerance;
    }

    std::string GetInfo(const Component* parentComponent = nullptr) const override
    {
        // For a component, we don't need the parent component parameter since it is the parent
        std::stringstream ss = {};
        ss << "Component: " << reference_designator << "\n";
        ss << "Value: " << value << "\n";
        ss << "Footprint: " << footprint_name << "\n";
        ss << "Position: (" << center_x << ", " << center_y << ")\n";
        ss << "Rotation: " << rotation << "°\n";
        ss << "Size: " << width << " x " << height << "\n";
        ss << "Layer: " << layer << "\n";
        ss << "Side: " << (side == MountingSide::kTop ? "Top" : "Bottom") << "\n";
        ss << "Type: " << (type == ComponentElementType::kSmd ? "SMD" : type == ComponentElementType::kThroughHole ? "Through Hole" : "Other") << "\n";
        ss << "Pins: " << pins.size() << "\n";
        return ss.str();
    }

    // Method to translate the component and its owned elements
    void Translate(double dx, double dy) override
    {
        center_x += dx;
        center_y += dy;

        // Translate all owned elements
        for (auto& pin : pins) {
            if (pin) {
                pin->Translate(dx, dy);
            }
        }

        for (auto& label : text_labels) {
            if (label) {
                label->Translate(dx, dy);
            }
        }

        for (auto& segment : graphical_elements) {
            segment.start.x_ax += dx;
            segment.start.y_ax += dy;
            segment.end.x_ax += dx;
            segment.end.y_ax += dy;
        }
    }
};

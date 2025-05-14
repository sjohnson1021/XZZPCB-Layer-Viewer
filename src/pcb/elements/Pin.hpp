#pragma once

#include <string>
#include <variant>
#include <vector> // If we later find a need for multiple outlines or complex shapes

// Define the individual pad shapes
struct CirclePad {
    double x_offset = 0.0; // Relative to Pin's main x, y
    double y_offset = 0.0; // Relative to Pin's main x, y
    double radius;
};

struct RectanglePad { // Can also represent a square
    double x_offset = 0.0;
    double y_offset = 0.0;
    double width;
    double height;
    // double rotation = 0.0; // Optional: if pads can be rotated
};

struct CapsulePad { // A rectangle with semicircular ends
    double x_offset = 0.0;
    double y_offset = 0.0;
    double width;  // Total width including semicircles
    double height; // Diameter of the semicircular ends / height of the rectangular part
    // double rotation = 0.0; // Optional
};

// The PadGeometry variant can hold any of the defined shapes
using PadShape = std::variant<CirclePad, RectanglePad, CapsulePad>;

// Enum for pin orientation if needed, similar to old XZZPCBLoader.h
enum class PinOrientation {
    Natural,    // Default
    Horizontal, // Long axis horizontal
    Vertical    // Long axis vertical
};

class Pin {
public:
    // --- Constructors ---
    Pin(double x, double y, const std::string& name, PadShape shape)
        : x_coord(x), y_coord(y), pin_name(name), pad_shape(shape) {}

    // --- Member Data ---
    double x_coord;
    double y_coord;
    std::string pin_name;
    int net_id = -1; // Placeholder for net association
    PadShape pad_shape;

    // From old Pin struct in XZZPCBLoader.h
    int layer = 0; // Assuming 0 for a default layer, to be defined better later
    int side = 0;  // e.g., 0 for top, 1 for bottom - needs clear definition
    std::string diode_reading;
    PinOrientation orientation = PinOrientation::Natural;
    // bool is_plated = true; // Example: if we need to distinguish PTH/NPTH for vias acting as pins

    // Any other members from the old Pin struct that are relevant:
    // e.g., thermal relief properties, solder mask/paste mask expansions, etc.

    // --- Member Functions ---
    // Potentially helpers to get bounding box, area, etc., based on pad_shape
    // double getBoundingWidth() const;
    // double getBoundingHeight() const;

private:
    // Internal helper data or functions
}; 
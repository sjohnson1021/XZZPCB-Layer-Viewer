#pragma once

#include <string>
#include <vector>
#include "Pin.hpp"       // Include the Pin definition
#include "TextLabel.hpp" // Include the TextLabel definition

// Helper struct for simple 2D points, could be moved to a common geometry header
struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

// Helper struct for line segments (e.g., for silkscreen or courtyard)
struct LineSegment {
    Point2D start;
    Point2D end;
    double thickness = 0.1;
    int layer = 0; // If segments can be on different layers relative to component
};

enum class ComponentType { SMD, ThroughHole, Other };
enum class MountingSide { Top, Bottom, Both }; // Consider if 'Both' is valid for a single component instance

class Component {
public:
    Component(const std::string& ref_des, const std::string& val, double x, double y)
        : reference_designator(ref_des), value(val), center_x(x), center_y(y) {}

    // Member Data
    std::string reference_designator; // e.g., "R1", "U100"
    std::string value;                // e.g., "10k", "ATMEGA328P"
    std::string footprint_name;       // e.g., "0805", "TQFP32"
    
    // Position and Orientation (typically of the component's geometric center)
    double center_x = 0.0;
    double center_y = 0.0;
    double rotation = 0.0; // Degrees

    // Bounding box (can be calculated from pins and silkscreen, or stored explicitly)
    // double width = 0.0; 
    // double height = 0.0;

    int layer = 0; // Primary layer the component resides on
    MountingSide side = MountingSide::Top;
    ComponentType type = ComponentType::SMD;

    std::vector<Pin> pins;                        // List of pins belonging to this component
    std::vector<TextLabel> text_labels;         // Text associated with this component (e.g., ref des, value on silk)
    std::vector<LineSegment> graphical_elements;  // For silkscreen, courtyard, assembly drawings, etc.

    // Add constructors, getters, setters, and helper methods as needed
    // void addPin(const Pin& pin);
    // void addTextLabel(const TextLabel& label);
    // void addGraphicalElement(const LineSegment& segment);
}; 
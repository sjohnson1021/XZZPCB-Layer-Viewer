#pragma once

#include <string>
#include <cstdint>

// Enum for text alignment/justification if needed later
// enum class TextJustification { TopLeft, TopCenter, TopRight, ... };
// enum class TextOrientation { Horizontal, Vertical, Rotated... };

class TextLabel {
public:
    TextLabel(const std::string& content, double pos_x, double pos_y, int layer_id, double size)
        : text_content(content), x(pos_x), y(pos_y), layer(layer_id), font_size(size) {}

    // Member Data
    std::string text_content;
    double x = 0.0;            // Anchor point X
    double y = 0.0;            // Anchor point Y
    int layer = 0;
    double font_size = 1.0;    // Font height or point size
    double scale = 1.0;        // General scaling factor (from original `scale`)
    double rotation = 0.0;     // Angle in degrees
    bool is_visible = true;    // From original `visibility`
    int ps06_flag = 0;         // From original `ps06flag`, meaning to be determined
    std::string font_family;   // e.g., "Arial", "Courier New"
    // TextJustification justification = TextJustification::TopLeft;
    // Color color;

    // Add constructors, getters, setters, and helper methods as needed
}; 
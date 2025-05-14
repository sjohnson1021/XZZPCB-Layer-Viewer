#pragma once

#include <cstdint>

class Trace {
public:
    Trace(int layer_id, double start_x, double start_y, double end_x, double end_y, double width_val)
        : layer(layer_id), x1(start_x), y1(start_y), x2(end_x), y2(end_y), width(width_val) {}

    // Member Data
    int layer = 0;
    double x1 = 0.0; // Start point X
    double y1 = 0.0; // Start point Y
    double x2 = 0.0; // End point X
    double y2 = 0.0; // End point Y
    double width = 0.1; // Trace width
    int net_id = -1;  // Associated net ID

    // Add constructors, getters, setters, and helper methods as needed
    // e.g., double getLength() const;
}; 
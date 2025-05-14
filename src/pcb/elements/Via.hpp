#pragma once

#include <string>
#include <cstdint>

class Via {
public:
    Via(double x_coord, double y_coord, int start_layer, int end_layer, 
        double radius_start_layer, double radius_end_layer)
        : x(x_coord), y(y_coord), layer_from(start_layer), layer_to(end_layer),
          pad_radius_from(radius_start_layer), pad_radius_to(radius_end_layer) {}

    // Member Data
    double x = 0.0;
    double y = 0.0;
    int layer_from = 0; // Starting layer index
    int layer_to = 0;   // Ending layer index
    
    double drill_diameter = 0.2; // Diameter of the drilled hole
    double pad_radius_from = 0.0; // Pad radius on the starting layer
    double pad_radius_to = 0.0;   // Pad radius on the ending layer
    // Or, if pads are always symmetrical: double pad_radius;

    int net_id = -1;           // Associated net ID
    std::string optional_text; // Optional text annotation

    // bool is_tented = false; // Example: for solder mask properties
    // bool is_filled = false; // Example: if via is filled (e.g. with conductive paste)

    // Add constructors, getters, setters, and helper methods as needed
}; 
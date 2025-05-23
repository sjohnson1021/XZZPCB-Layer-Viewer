#pragma once

#include <string>
#include <cstdint> // For fixed-width types if absolutely needed, or use standard types

class Arc {
public:
    // Assuming x_center, y_center are the arc's center point
    Arc(int layer_id, double x_center, double y_center, double radius_val, 
        double start_angle_deg, double end_angle_deg)
        : layer(layer_id), cx(x_center), cy(y_center), radius(radius_val),
          start_angle(start_angle_deg), end_angle(end_angle_deg) {}

    // Member Data
    int layer = 0;
    double cx = 0.0;
    double cy = 0.0;
    double radius = 0.0;
    double start_angle = 0.0; // Degrees
    double end_angle = 0.0;   // Degrees
    // int32_t scale; // Original field from XZZPCBLoader, meaning unclear, might be width/thickness
    double thickness = 1.0; // Assuming scale might relate to thickness, making it more explicit
    int net_id = -1;        // Associated net ID

    // Add constructors, getters, setters, and helper methods as needed
    double GetCX() const { return cx; }
    double GetCY() const { return cy; }
    double GetRadius() const { return radius; }
    double GetStartAngle() const { return start_angle; }
    double GetEndAngle() const { return end_angle; }
    double GetThickness() const { return thickness; }
    int GetNetId() const { return net_id; }
    int GetLayer() const { return layer; }
}; 
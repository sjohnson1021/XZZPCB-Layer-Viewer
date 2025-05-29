#pragma once

#include "pcb/elements/Element.hpp" // Include base class
#include "utils/Vec2.hpp"           // For Vec2
#include <string>
// #include <cstdint> // No longer strictly needed

class Arc : public Element
{ // Inherit from Element
public:
    Arc(int layer_id, double x_center, double y_center, double radius_val,
        double start_angle_deg, double end_angle_deg, double thickness_val = 1.0, int net_id_val = -1)
        : Element(layer_id, ElementType::ARC, net_id_val), // Call base constructor
          cx(x_center), cy(y_center), radius(radius_val),
          start_angle(start_angle_deg), end_angle(end_angle_deg), thickness(thickness_val)
    {
    }

    // Copy constructor
    Arc(const Arc &other)
        : Element(other.m_layerId, other.m_type, other.m_netId), // Initialize Element base
          cx(other.cx),
          cy(other.cy),
          radius(other.radius),
          start_angle(other.start_angle),
          end_angle(other.end_angle),
          thickness(other.thickness)
    {
        setVisible(other.isVisible()); // Copy visibility state
    }

    // --- Overridden virtual methods ---
    BLRect getBoundingBox(const Component *parentComponent = nullptr) const override;
    bool isHit(const Vec2 &worldMousePos, float tolerance, const Component *parentComponent = nullptr) const override;
    std::string getInfo(const Component *parentComponent = nullptr) const override;
    void translate(double dx, double dy) override;

    // --- Arc-specific Member Data ---
    double cx = 0.0;
    double cy = 0.0;
    double radius = 0.0;
    double start_angle = 0.0; // Degrees
    double end_angle = 0.0;   // Degrees
    double thickness = 1.0;
    // layer and net_id are in Element

    // --- Arc-specific Getters ---
    double GetCX() const { return cx; }
    double GetCY() const { return cy; }
    double GetRadius() const { return radius; }
    double GetStartAngle() const { return start_angle; }
    double GetEndAngle() const { return end_angle; }
    double GetThickness() const { return thickness; }
    // GetLayerId() and GetNetId() are inherited
};
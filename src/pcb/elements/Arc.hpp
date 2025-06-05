#pragma once

#include <string>

#include "pcb/elements/Element.hpp"  // Include base class
#include "utils/Vec2.hpp"            // For Vec2
// #include <cstdint> // No longer strictly needed

class Arc : public Element
{  // Inherit from Element
public:
    Arc(int layer_id, Vec2 center_val, double radius_val, double start_angle_deg, double end_angle_deg, double thickness_val = 1.0, int net_id_val = -1)
        : Element(layer_id, ElementType::kArc, net_id_val),  // Call base constructor
          center(center_val),
          radius(radius_val),
          start_angle(start_angle_deg),
          end_angle(end_angle_deg),
          thickness(thickness_val)
    {
    }

    // Copy constructor
    Arc(const Arc& other)
        : Element(other.GetLayerId(), other.GetElementType(), other.GetNetId()),  // Initialize Element base
          center(other.center),
          radius(other.radius),
          start_angle(other.start_angle),
          end_angle(other.end_angle),
          thickness(other.thickness)
    {
        SetVisible(other.IsVisible());  // Copy visibility state
    }

    // --- Overridden virtual methods ---
    BLRect GetBoundingBox(const Component* parent_component = nullptr) const override;
    bool IsHit(const Vec2& world_mouse_pos, float tolerance, const Component* parent_component = nullptr) const override;
    std::string GetInfo(const Component* parent_component = nullptr) const override;
    void Translate(double dist_x, double dist_y) override;

    // --- Arc-specific Member Data ---
    Vec2 center {};
    double radius = 0.0;
    double start_angle = 0.0;  // Degrees
    double end_angle = 0.0;    // Degrees
    double thickness = 1.0;
    // layer and net_id are in Element

    // --- Arc-specific Getters ---
    [[nodiscard]] double GetCenterX() const { return center.x_ax; }
    [[nodiscard]] double GetCenterY() const { return center.y_ax; }
    [[nodiscard]] double GetRadius() const { return radius; }
    [[nodiscard]] double GetStartAngle() const { return start_angle; }
    [[nodiscard]] double GetEndAngle() const { return end_angle; }
    [[nodiscard]] double GetThickness() const { return thickness; }
    // GetLayerId() and GetNetId() are inherited
};

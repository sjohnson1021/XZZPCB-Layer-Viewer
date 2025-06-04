#pragma once

#include <cmath>  // For sqrt, atan2
#include <string>

#include "Element.hpp"  // Include base class

#include "utils/Vec2.hpp"  // For Vec2
// #include <cstdint> // No longer strictly needed here if Element handles basic types

class Trace : public Element
{  // Inherit from Element
public:
    Trace(int layer_id, Vec2 start_point, Vec2 end_point, double width_val, int net_id_val = -1)
        : Element(layer_id, ElementType::kTrace, net_id_val),  // Call base constructor
          x1(start_point.x_ax),
          y1(start_point.y_ax),
          x2(end_point.x_ax),
          y2(end_point.y_ax),
          width(width_val)
    {
    }

    // Copy constructor
    Trace(const Trace& other)
        : Element(other.GetLayerId(), other.GetElementType(), other.GetNetId()),  // Initialize Element base
          x1(other.x1),
          y1(other.y1),
          x2(other.x2),
          y2(other.y2),
          width(other.width)
    {
        SetVisible(other.IsVisible());  // Copy visibility state
    }

    // --- Overridden virtual methods ---
    BLRect GetBoundingBox(const Component* parent_component = nullptr) const override;
    bool IsHit(const Vec2& world_mouse_pos, float tolerance, const Component* parent_component = nullptr) const override;
    std::string GetInfo(const Component* parent_component = nullptr) const override;
    void Translate(double dist_x, double dist_y) override;

    // --- Trace-specific Member Data --- (layer and net_id are now in Element)
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    double width = 0.1;

    // --- Trace-specific Getters ---
    [[nodiscard]] double GetStartX() const { return x1; }
    [[nodiscard]] double GetStartY() const { return y1; }
    [[nodiscard]] double GetEndX() const { return x2; }
    [[nodiscard]] double GetEndY() const { return y2; }
    [[nodiscard]] double GetWidth() const { return width; }
    // GetLayerId() and GetNetId() are inherited from Element

    // --- Trace-specific Helper Methods ---
    [[nodiscard]] double GetLength() const { return sqrt(((x2 - x1) * (x2 - x1)) + ((y2 - y1) * (y2 - y1))); }
    [[nodiscard]] double GetAngle() const { return atan2(y2 - y1, x2 - x1); }
    [[nodiscard]] double GetCenterX() const { return (x1 + x2) / 2.0; }
    [[nodiscard]] double GetCenterY() const { return (y1 + y2) / 2.0; }
    [[nodiscard]] double GetMidpointX() const { return (x1 + x2) / 2.0; }
    [[nodiscard]] double GetMidpointY() const { return (y1 + y2) / 2.0; }
    [[nodiscard]] double GetAngleTo(double x, double y) const { return atan2(y - GetMidpointY(), x - GetMidpointX()); }
};

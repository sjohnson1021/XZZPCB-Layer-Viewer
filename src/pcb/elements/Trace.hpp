#pragma once

#include "pcb/elements/Element.hpp" // Include base class
#include "utils/Vec2.hpp"           // For Vec2
#include <cmath>                    // For sqrt, atan2
// #include <cstdint> // No longer strictly needed here if Element handles basic types

class Trace : public Element
{ // Inherit from Element
public:
    Trace(int layer_id, double start_x, double start_y, double end_x, double end_y, double width_val, int net_id_val = -1)
        : Element(layer_id, ElementType::TRACE, net_id_val), // Call base constructor
          x1(start_x), y1(start_y), x2(end_x), y2(end_y), width(width_val)
    {
    }

    // Copy constructor
    Trace(const Trace &other)
        : Element(other.m_layerId, other.m_type, other.m_netId), // Initialize Element base
          x1(other.x1),
          y1(other.y1),
          x2(other.x2),
          y2(other.y2),
          width(other.width)
    {
        setVisible(other.isVisible()); // Copy visibility state
    }

    // --- Overridden virtual methods ---
    BLRect getBoundingBox(const Component *parentComponent = nullptr) const override;
    bool isHit(const Vec2 &worldMousePos, float tolerance, const Component *parentComponent = nullptr) const override;
    std::string getInfo(const Component *parentComponent = nullptr) const override;
    void translate(double dx, double dy) override;

    // --- Trace-specific Member Data --- (layer and net_id are now in Element)
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    double width = 0.1;

    // --- Trace-specific Getters ---
    double GetStartX() const { return x1; }
    double GetStartY() const { return y1; }
    double GetEndX() const { return x2; }
    double GetEndY() const { return y2; }
    double GetWidth() const { return width; }
    // GetLayerId() and GetNetId() are inherited from Element

    // --- Trace-specific Helper Methods ---
    double getLength() const { return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)); }
    double getAngle() const { return atan2(y2 - y1, x2 - x1); }
    double getCenterX() const { return (x1 + x2) / 2.0; }
    double getCenterY() const { return (y1 + y2) / 2.0; }
    double getMidpointX() const { return (x1 + x2) / 2.0; }
    double getMidpointY() const { return (y1 + y2) / 2.0; }
    double getAngleTo(double x, double y) const { return atan2(y - getMidpointY(), x - getMidpointX()); }
};
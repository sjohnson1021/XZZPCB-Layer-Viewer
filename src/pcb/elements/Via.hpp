#pragma once

#include <string>

#include "Element.hpp"  // Include base class
#include "utils/Vec2.hpp"            // For Vec2
// #include <cstdint> // No longer strictly needed

class Via : public Element
{  // Inherit from Element
public:
    Via(double x_coord, double y_coord, int start_layer, int end_layer, double drill_dia, double radius_start_layer, double radius_end_layer, int net_id_val = -1, const std::string& text = "")
        : Element(start_layer, ElementType::kVia, net_id_val),  // Use start_layer as primary layer for Element
          x(x_coord),
          y(y_coord),
          layer_from(start_layer),
          layer_to(end_layer),
          drill_diameter(drill_dia),
          pad_radius_from(radius_start_layer),
          pad_radius_to(radius_end_layer),
          optional_text(text)
    {
        {
        }
    }

    // Copy constructor
    Via(const Via& other)
        : Element(other.GetLayerId(), other.GetElementType(), other.GetNetId()),  // Initialize Element base
          x(other.x),
          y(other.y),
          layer_from(other.layer_from),
          layer_to(other.layer_to),
          drill_diameter(other.drill_diameter),
          pad_radius_from(other.pad_radius_from),
          pad_radius_to(other.pad_radius_to),
          optional_text(other.optional_text)
    {
        SetVisible(other.IsVisible());  // Copy visibility state
    }

    // --- Overridden virtual methods ---
    BLRect GetBoundingBox(const Component* parent_component = nullptr) const override;
    bool IsHit(const Vec2& world_mouse_pos, float tolerance, const Component* parent_component = nullptr) const override;
    std::string GetInfo(const Component* parent_component = nullptr, const Board* board = nullptr) const override;
    void Translate(double dx, double dy) override;  // Declaration only
    void Mirror(double center_axis) override;

    // --- Via-specific Member Data ---
    double x = 0.0;
    double y = 0.0;
    // int layer = 30; // This 'layer' member was a bit ambiguous for vias; m_layerId from Element now holds primary layer
    int layer_from = 0;
    int layer_to = 0;
    double drill_diameter = 0.2;
    double pad_radius_from = 0.0;
    double pad_radius_to = 0.0;
    std::string optional_text;
    // net_id is in Element

    // --- Via-specific Getters ---
    [[nodiscard]] double GetX() const { return x; }
    [[nodiscard]] double GetY() const { return y; }
    [[nodiscard]] int GetLayerFrom() const { return layer_from; }
    [[nodiscard]] int GetLayerTo() const { return layer_to; }
    [[nodiscard]] double GetPadRadiusFrom() const { return pad_radius_from; }
    [[nodiscard]] double GetPadRadiusTo() const { return pad_radius_to; }
    [[nodiscard]] double GetDrillDiameter() const { return drill_diameter; }
    [[nodiscard]] const std::string& GetOptionalText() const { return optional_text; }
    // GetLayerId() (primary) and GetNetId() are inherited

    [[nodiscard]] bool IsOnLayer(int query_layer_id) const { return query_layer_id >= layer_from && query_layer_id <= layer_to; }
};

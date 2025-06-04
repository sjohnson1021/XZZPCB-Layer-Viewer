#pragma once

#include <string>
#include <utility>

#include "Element.hpp"  // Include base class

#include "utils/Vec2.hpp"  // For Vec2
// #include <cstdint> // No longer strictly needed

// Enum for text alignment/justification if needed later
// enum class TextJustification { TopLeft, TopCenter, TopRight, ... };
// enum class TextOrientation { Horizontal, Vertical, Rotated... };

class TextLabel : public Element
{  // Inherit from Element
public:
    TextLabel(std::string content,
              Vec2 coords,
              int layer_id,
              double font_size_val,
              double scale_val = 1.0,
              double rotation_val = 0.0,
              const std::string& font_family_val = "",
              int net_id_val = -1)                                 // Net ID typically -1 for text
        : Element(layer_id, ElementType::kTextLabel, net_id_val),  // Call base constructor
          text_content(std::move(content)),
          coords(coords),
          font_size(font_size_val),
          scale(scale_val),
          rotation(rotation_val),
          font_family(font_family_val)
    {
        // Element's m_isGloballyVisible handles the 'is_visible' flag
    }

    // Copy constructor
    TextLabel(const TextLabel& other)
        : Element(other.GetLayerId(), other.GetElementType(), other.GetNetId()),  // Initialize Element base
          text_content(other.text_content),
          coords(other.coords),
          font_size(other.font_size),
          scale(other.scale),
          rotation(other.rotation),
          font_family(other.font_family)
    {
        SetVisible(true);  // Copy visibility state
    }

    // --- Overridden virtual methods ---
    BLRect GetBoundingBox(const Component* parent_component = nullptr) const override;
    bool IsHit(const Vec2& world_mouse_pos, float tolerance, const Component* parent_component = nullptr) const override;
    std::string GetInfo(const Component* parent_component = nullptr) const override;
    void Translate(double dist_x, double dist_y) override;

    // --- TextLabel-specific Member Data ---
    std::string text_content;
    Vec2 coords = {0.0, 0.0};
    double font_size = 1.0;
    double scale = 1.0;
    double rotation = 0.0;
    // int ps06_flag = 0; // Retain if its meaning/use is found
    std::string font_family;
    // layer, net_id, is_visible are in Element

    // --- TextLabel-specific Getters ---
    // ... (if any are needed beyond what Element provides or what's directly public)

    // Note: Original 'is_visible' is now Element::m_isGloballyVisible, accessible via Element::isVisible()
    // Original 'layer' is Element::m_layerId, accessible via Element::getLayerId()

    // Add constructors, getters, setters, and helper methods as needed
};

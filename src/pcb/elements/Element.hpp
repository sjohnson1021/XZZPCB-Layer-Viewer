#ifndef ELEMENT_HPP
#define ELEMENT_HPP

#include <string>

#include <blend2d.h>  // For BLRect

#include "utils/Vec2.hpp"  // For Vec2

// Forward declaration if Board context is needed by some virtual methods
class Board;
class Component;  // Added: Parent component context for pins

enum class ElementType {
    kNone,
    kTrace,
    kVia,
    kArc,
    kPin,
    kTextLabel,
    kComponentGraphic,
    kComponent  // For generic graphical lines/shapes within a component, if needed
    // Add other types as necessary
};

class Element
{
public:
    Element(int layer_id, ElementType type, int net_id = -1) : m_layer_id_(layer_id), m_type_(type), m_net_id_(net_id) {}

    virtual ~Element() = default;

    // Pure virtual methods to be implemented by derived classes
    virtual BLRect GetBoundingBox(const Component* parent_component = nullptr) const = 0;
    virtual bool IsHit(const Vec2& world_mouse_pos, float tolerance, const Component* parent_component = nullptr) const = 0;
    virtual std::string GetInfo(const Component* parent_component = nullptr, const class Board* board = nullptr) const = 0;

    // Method to translate the element's coordinates
    virtual void Translate(double dx, double dy) = 0;

    // Method to mirror the element's coordinates around a vertical axis
    virtual void Mirror(double center_axis) = 0;

    // Concrete methods
    [[nodiscard]] ElementType GetElementType() const { return m_type_; }
    [[nodiscard]] int GetLayerId() const { return m_layer_id_; }
    [[nodiscard]] int GetNetId() const { return m_net_id_; }
    void SetNetId(int net_id) { m_net_id_ = net_id; }
	void SetLayerId(int layer_id) { m_layer_id_ = layer_id; }

    [[nodiscard]] bool IsVisible() const { return m_is_globally_visible_; }
    void SetVisible(bool visible) { m_is_globally_visible_ = visible; }

    Element(const Element&) = delete;
    Element& operator=(const Element&) = delete;
    Element(Element&&) = delete;
    Element& operator=(Element&&) = delete;

private:
    int m_layer_id_;
    ElementType m_type_;
    int m_net_id_;
    bool m_is_globally_visible_ {true};
};

#endif  // ELEMENT_HPP

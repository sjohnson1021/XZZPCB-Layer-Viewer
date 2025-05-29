#ifndef ELEMENT_HPP
#define ELEMENT_HPP

#include <string>
#include <memory>         // For std::unique_ptr if used in containers
#include "utils/Vec2.hpp" // For Vec2
#include <blend2d.h>      // For BLRect

// Forward declaration if Board context is needed by some virtual methods
class Board;
class Component; // Added: Parent component context for pins

enum class ElementType
{
    NONE,
    TRACE,
    VIA,
    ARC,
    PIN,
    TEXT_LABEL,
    COMPONENT_GRAPHIC,
    COMPONENT // For generic graphical lines/shapes within a component, if needed
    // Add other types as necessary
};

class Element
{
public:
    Element(int layerId, ElementType type, int netId = -1)
        : m_layerId(layerId), m_type(type), m_netId(netId), m_isGloballyVisible(true) {}

    virtual ~Element() = default;

    // Pure virtual methods to be implemented by derived classes
    virtual BLRect getBoundingBox(const Component *parentComponent = nullptr) const = 0;
    virtual bool isHit(const Vec2 &worldMousePos, float tolerance, const Component *parentComponent = nullptr) const = 0;
    virtual std::string getInfo(const Component *parentComponent = nullptr) const = 0;

    // Method to translate the element's coordinates
    virtual void translate(double dx, double dy) = 0;

    // Concrete methods
    ElementType getElementType() const { return m_type; }
    int getLayerId() const { return m_layerId; }
    int getNetId() const { return m_netId; }
    void setNetId(int netId) { m_netId = netId; }

    bool isVisible() const { return m_isGloballyVisible; }
    void setVisible(bool visible) { m_isGloballyVisible = visible; }

    Element(const Element &) = delete;
    Element &operator=(const Element &) = delete;
    Element(Element &&) = delete;
    Element &operator=(Element &&) = delete;

protected:
    int m_layerId;
    ElementType m_type;
    int m_netId;
    bool m_isGloballyVisible;
};

#endif // ELEMENT_HPP
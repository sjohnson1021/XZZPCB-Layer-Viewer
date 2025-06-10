#include "pcb/elements/Element.hpp"
#include "pcb/elements/Component.hpp"  // For MountingSide enum definition

// Element constructor implementation
Element::Element(int layer_id, ElementType type, int net_id) 
    : m_layer_id_(layer_id), m_type_(type), m_net_id_(net_id), m_board_side_(MountingSide::kTop)
{
    // Default initialization - board side defaults to top
}

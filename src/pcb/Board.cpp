#include "pcb/Board.hpp"
#include "pcb/BoardLoaderFactory.hpp" // Include the factory
#include "core/BoardDataManager.hpp"  // Include for implementation
#include <iostream>                   // For std::cerr or logging
#include <algorithm>                  // For std::min/max in GetBoundingBox
#include <vector>

// Include concrete element types for make_unique
#include "pcb/elements/Arc.hpp"
#include "pcb/elements/Via.hpp"
#include "pcb/elements/Trace.hpp"
#include "pcb/elements/TextLabel.hpp"
// Component.hpp and Net.hpp are already included via Board.hpp (as they are not just Element types)

// Default constructor
Board::Board() : m_isLoaded(false)
{
    // Default initialization only
}

// Constructor that takes a file path - now just calls initialize
Board::Board(const std::string &filePath)
    : file_path(filePath), m_isLoaded(false)
{
    initialize(filePath);
}

// New initialization method that can be called by both constructor and loaders
bool Board::initialize(const std::string &filePath)
{
    if (filePath.empty())
    {
        m_errorMessage = "File path is empty.";
        m_isLoaded = false;
        return false;
    }

    if (filePath == "dummy_fail.pcb")
    {
        m_errorMessage = "This is a dummy failure to test the error modal.";
        m_isLoaded = false;
        return false;
    }

    // Automatically normalize coordinates after loading
    BLRect bounds = GetBoundingBox(true);
    if (bounds.w > 0 && bounds.h > 0)
    {
        NormalizeCoordinatesAndGetCenterOffset(bounds);
    }
    else
    {
        std::cerr << "Warning: Could not determine valid bounding box for normalization for file: " << filePath << std::endl;
    }

    m_isLoaded = true;
    m_errorMessage.clear();
    return true;
}

// --- Add Methods ---
void Board::addArc(const Arc &arc)
{
    m_elementsByLayer[arc.getLayerId()].emplace_back(std::make_unique<Arc>(arc));
}
void Board::addVia(const Via &via)
{
    // Vias can span multiple layers. Add to m_elementsByLayer based on their primary layer id.
    // The IsOnLayer() check within Via itself handles multi-layer aspect for rendering/interaction.
    m_elementsByLayer[via.getLayerId()].emplace_back(std::make_unique<Via>(via));
}
void Board::addTrace(const Trace &trace)
{
    m_elementsByLayer[trace.getLayerId()].emplace_back(std::make_unique<Trace>(trace));
}
void Board::addStandaloneTextLabel(const TextLabel &label)
{
    m_elementsByLayer[label.getLayerId()].emplace_back(std::make_unique<TextLabel>(label));
}
void Board::addComponent(const Component &component)
{
    m_components.push_back(component); // Components stored directly
}
void Board::addNet(const Net &net)
{
    m_nets.emplace(net.GetId(), net); // Nets stored directly
}
void Board::addLayer(const LayerInfo &layer)
{
    layers.push_back(layer);
}

// --- Layer Access Methods ---
std::vector<LayerInfo> Board::GetLayers() const
{
    return layers;
}

int Board::GetLayerCount() const
{
    return static_cast<int>(layers.size());
}

std::string Board::GetLayerName(int layerIndex) const
{
    if (layerIndex >= 0 && layerIndex < layers.size())
    {
        return layers[layerIndex].name;
    }
    return "Invalid Layer Index"; // Or throw an exception
}

bool Board::IsLayerVisible(int layerIndex) const
{
    if (layerIndex >= 0 && layerIndex < layers.size())
    {
        return layers[layerIndex].is_visible;
    }
    return false; // Or throw an exception
}

void Board::SetLayerVisible(int layerIndex, bool visible)
{
    if (layerIndex >= 0 && layerIndex < layers.size())
    {
        layers[layerIndex].is_visible = visible;
        // Use BoardDataManager if available
        if (m_boardDataManager)
        {
            m_boardDataManager->SetLayerVisible(layerIndex, visible);
        }
    }
    // Else: handle error or do nothing for invalid index
}

void Board::SetLayerColor(int layerIndex, BLRgba32 color)
{
    // No-op: color is now managed by BoardDataManager
}

// --- Loading Status Methods ---
bool Board::IsLoaded() const
{
    return m_isLoaded;
}

std::string Board::GetErrorMessage() const
{
    return m_errorMessage;
}

std::string Board::GetFilePath() const
{
    return file_path;
}

const LayerInfo *Board::GetLayerById(int layerId) const
{
    for (const auto &layer : layers)
    {
        if (layer.id == layerId)
        {
            return &layer;
        }
    }
    return nullptr; // Not found
}

// Calculate the bounding box based on elements on Layer 28 (Board Outline for XZZ)
BLRect Board::GetBoundingBox(bool include_invisible_layers) const
{
    bool first = true;
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    const int board_outline_layer_id = 28; // XZZ specific

    const LayerInfo *outline_layer_info = GetLayerById(board_outline_layer_id);

    if (!outline_layer_info)
    {
        std::cerr << "Warning: Board outline layer ID " << board_outline_layer_id << " not found in layer definitions." << std::endl;
        return BLRect(0, 0, 0, 0); // Layer 28 not defined, cannot determine outline.
    }

    if (!include_invisible_layers && !outline_layer_info->IsVisible())
    {
        // Layer 28 is not visible and we are not including invisible layers.
        return BLRect(0, 0, 0, 0);
    }

    auto expand_rect_from_point = [&](double x, double y)
    {
        if (first)
        {
            min_x = max_x = x;
            min_y = max_y = y;
            first = false;
        }
        else
        {
            if (x < min_x)
                min_x = x;
            if (x > max_x)
                max_x = x;
            if (y < min_y)
                min_y = y;
            if (y > max_y)
                max_y = y;
        }
    };

    auto expand_rect_from_blrect = [&](const BLRect &r)
    {
        if (r.w <= 0 || r.h <= 0)
            return; // Skip invalid rects
        expand_rect_from_point(r.x, r.y);
        expand_rect_from_point(r.x + r.w, r.y + r.h);
    };

    auto it = m_elementsByLayer.find(board_outline_layer_id);
    if (it != m_elementsByLayer.end())
    {
        for (const auto &element_ptr : it->second)
        {
            if (!element_ptr)
                continue;

            // All elements on the outline layer should contribute.
            // The Element::getBoundingBox() should provide the necessary extent.
            // We pass nullptr for parentComponent as these are standalone elements.
            BLRect elem_bounds = element_ptr->getBoundingBox(nullptr);
            expand_rect_from_blrect(elem_bounds);
        }
    }
    else
    {
        std::cerr << "Warning: No elements found on board outline layer ID " << board_outline_layer_id << "." << std::endl;
    }

    if (first)
    { // No elements found on layer 28, or they all had invalid bounding boxes
        std::cerr << "Warning: Could not determine board bounds from outline layer " << board_outline_layer_id << ". Resulting bounds are empty." << std::endl;
        return BLRect(0, 0, 0, 0);
    }
    return BLRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

BoardPoint2D Board::NormalizeCoordinatesAndGetCenterOffset(const BLRect &original_bounds)
{
    if (original_bounds.w <= 0 || original_bounds.h <= 0)
    {
        // std::cerr << "NormalizeCoordinatesAndGetCenterOffset: Original bounds invalid, skipping normalization." << std::endl;
        return {0.0, 0.0}; // No valid bounds to normalize against
    }

    double offset_x = original_bounds.x + original_bounds.w / 2.0;
    double offset_y = original_bounds.y + original_bounds.h / 2.0;

    // std::cout << "Normalizing by offset: X=" << offset_x << ", Y=" << offset_y << std::endl;

    // Normalize all standalone elements in m_elementsByLayer
    for (auto &layer_pair : m_elementsByLayer)
    {
        for (auto &element_ptr : layer_pair.second)
        {
            if (element_ptr)
            {
                element_ptr->translate(-offset_x, -offset_y);
            }
        }
    }

    // Normalize Components and their sub-elements (pins, component graphics, component text labels)
    // The Component::translate method should handle its children.
    for (auto &comp : m_components)
    {
        comp.translate(-offset_x, -offset_y);
    }

    // Update the board's own origin_offset to store this normalization offset
    this->origin_offset = {offset_x, offset_y};

    // The board's width and height are derived from the original_bounds,
    // they don't change due to normalization, only their position relative to (0,0).
    this->width = original_bounds.w;
    this->height = original_bounds.h;

    // std::cout << "Board dimensions after norm: W=" << this->width << ", H=" << this->height << std::endl;
    // BLRect new_bounds_check = GetBoundingBox(true);
    // std::cout << "New bounds after norm: X=" << new_bounds_check.x << " Y=" << new_bounds_check.y << " W=" << new_bounds_check.w << " H=" << new_bounds_check.h << std::endl;

    return {offset_x, offset_y};
}

// If PcbLoader is used internally by Board::ParseBoardFile (example skeleton)
// ... existing code ...

// --- GetAllVisibleElementsForInteraction (New Implementation) ---
std::vector<ElementInteractionInfo> Board::GetAllVisibleElementsForInteraction() const
{
    std::vector<ElementInteractionInfo> result;
    result.reserve(5000); // Pre-allocate, adjust based on typical board size

    // 1. Iterate through standalone elements grouped by layer
    for (const auto &layer_pair : m_elementsByLayer)
    {
        const LayerInfo *layer_info = GetLayerById(layer_pair.first);
        if (layer_info && layer_info->IsVisible())
        {
            for (const auto &element_ptr : layer_pair.second)
            {
                if (element_ptr && element_ptr->isVisible()) // Assuming Element base has isVisible()
                {
                    result.push_back({element_ptr.get(), nullptr});
                }
            }
        }
        else
        {
            std::cout << "Board: Layer " << layer_pair.first << " is not visible or not found" << std::endl;
        }
    }

    // 2. Iterate through components and their elements (Pins, component-specific TextLabels)
    for (const auto &comp : m_components) // comp is const Component&
    {
        const LayerInfo *comp_layer_info = GetLayerById(comp.layer); // Component's primary layer

        if (comp_layer_info && comp_layer_info->IsVisible())
        {
            // Add Pins of the component
            for (const auto &pin_ptr : comp.pins) // pin_ptr is std::unique_ptr<Pin>
            {
                if (!pin_ptr)
                    continue;
                const LayerInfo *pin_layer_info = GetLayerById(pin_ptr->getLayerId());
                // Assuming Pin has isVisible() and getLayerId()
                if (pin_ptr->isVisible() && pin_layer_info && pin_layer_info->IsVisible())
                {
                    result.push_back({pin_ptr.get(), &comp}); // Use .get() for unique_ptr
                }
                else
                {
                }
            }

            // Add TextLabels of the component
            for (const auto &label_ptr : comp.text_labels) // label_ptr is std::unique_ptr<TextLabel>
            {
                if (!label_ptr)
                    continue;
                const LayerInfo *label_layer_info = GetLayerById(label_ptr->getLayerId());
                // Assuming TextLabel has isVisible() and getLayerId()
                if (label_ptr->isVisible() && label_layer_info && label_layer_info->IsVisible())
                {
                    result.push_back({label_ptr.get(), &comp}); // Use .get() for unique_ptr
                }
                else
                {
                    std::cout << "Board: TextLabel on layer " << label_ptr->getLayerId() << " is not visible or layer not found" << std::endl;
                }
            }
        }
        else
        {
            std::cout << "Board: Component layer is not visible or not found" << std::endl;
        }
    }
    return result;
}

const Net *Board::getNetById(int net_id) const
{
    auto it = m_nets.find(net_id);
    if (it != m_nets.end())
    {
        return &(it->second);
    }
    return nullptr;
}

void Board::SetBoardDataManager(std::shared_ptr<BoardDataManager> manager)
{
    m_boardDataManager = manager;
}
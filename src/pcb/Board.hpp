#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>       // For std::map
#include <memory>    // For std::unique_ptr
#include <blend2d.h> // Added for BLRgba32

#include "elements/Element.hpp"   // Base class for all elements
#include "elements/Component.hpp" // Components are containers, not Elements themselves directly
#include "elements/Net.hpp"       // Nets are metadata

// Forward declarations
class BoardDataManager;
class Arc;
class Via;
class Trace;
class TextLabel;
// Component is already included

// Structure to define a layer
struct LayerInfo
{
    enum class LayerType
    {
        Signal,
        PowerPlane, // Could be positive or negative plane
        Silkscreen,
        SolderMask,
        SolderPaste,
        Drill,
        Mechanical,
        BoardOutline,
        Comment,
        Other
    };

    int id = 0;       // Original ID from file if applicable, or internal ID
    std::string name; // e.g., "TopLayer", "BottomLayer", "SilkscreenTop"
    LayerType type = LayerType::Other;
    bool is_visible = true;
    BLRgba32 color = BLRgba32(0xFFFFFFFF); // Default to white, ensure BLRgba32 is known (include Blend2D headers if not already via other includes)
    // double thickness; // Optional: physical thickness of the layer

    LayerInfo(int i, const std::string &n, LayerType t, BLRgba32 c = BLRgba32(0xFFFFFFFF))
        : id(i), name(n), type(t), is_visible(true), color(c) {}
    // Default constructor for LayerInfo if needed by containers, ensure it initializes color.
    LayerInfo() : id(-1), name("Unknown"), type(LayerType::Other), is_visible(true), color(0xFF000000) {} // Default to black for uninitialized

    bool IsVisible() const { return is_visible; }
    void SetVisible(bool visible) { is_visible = visible; }
    int GetId() const { return id; }
    const std::string &GetName() const { return name; }
    LayerType GetType() const { return type; }
    BLRgba32 GetColor() const { return color; }
};
struct BoardPoint2D
{
    double x = 0.0;
    double y = 0.0;
};

// New struct for interaction queries
struct ElementInteractionInfo
{
    const Element *element = nullptr;
    const Component *parentComponent = nullptr; // Null if not part of a component or if element is a Component's graphical_element
};

class Board
{
public:
    // Constructor that takes a file path (implementation will be in Board.cpp)
    explicit Board(const std::string &filePath);
    Board(); // Keep default constructor if needed, or remove if filePath constructor is primary

    // New initialization method
    bool initialize(const std::string &filePath);

    // Set the BoardDataManager for handling layer visibility changes
    void SetBoardDataManager(std::shared_ptr<BoardDataManager> manager);

    // --- Board Metadata ---
    std::string board_name;
    std::string file_path; // Path to the original PCB file

    // Board dimensions (could be calculated from elements or read from file)
    double width = 0.0;
    double height = 0.0;
    // Point2D origin_offset = {0.0, 0.0}; // If coordinates need global adjustment. Requires Point2D to be defined or included.
    // For now, let's assume Point2D from Component.hpp is not directly used here or define a simple one.
    BoardPoint2D origin_offset = {0.0, 0.0};

    std::vector<LayerInfo> layers;

    // --- NEW PCB Element Storage ---
    // Elements are grouped by layer ID for efficient layer-based operations.
    // Components are stored separately as they are containers of other elements (pins, specific text).
    std::map<int, std::vector<std::unique_ptr<Element>>> m_elementsByLayer;
    std::vector<Component> m_components; // Keep storing components as before
    std::unordered_map<int, Net> m_nets; // Keep storing nets as before

    // --- Methods to add elements (modified) ---
    // These will now emplace std::unique_ptr<Element> into m_elementsByLayer
    void addArc(const Arc &arc);
    void addVia(const Via &via);
    void addTrace(const Trace &trace);
    void addStandaloneTextLabel(const TextLabel &label); // For text not part of a component
    void addComponent(const Component &component);       // Will store in m_components
    void addNet(const Net &net);                         // Will store in m_nets
    void addLayer(const LayerInfo &layer);               // Will store in layers

    // --- Methods to retrieve elements (modified/new) ---
    const std::vector<Component> &GetComponents() const { return m_components; }
    // Old GetTraces, GetVias, etc. are removed. Use GetAllVisibleElementsForInteraction or iterate m_elementsByLayer if needed.

    const Net *getNetById(int net_id) const;
    const LayerInfo *GetLayerById(int layerId) const;

    std::vector<ElementInteractionInfo> GetAllVisibleElementsForInteraction() const;

    // --- Layer Access Methods ---
    std::vector<LayerInfo> GetLayers() const;
    int GetLayerCount() const;
    std::string GetLayerName(int layerIndex) const; // Consider returning const&
    bool IsLayerVisible(int layerIndex) const;
    void SetLayerVisible(int layerIndex, bool visible);
    void SetLayerColor(int layerIndex, BLRgba32 color);

    // --- Loading Status Methods ---
    bool IsLoaded() const;
    std::string GetErrorMessage() const; // Consider returning const&
    std::string GetFilePath() const;     // Getter for file_path

    // Calculate the bounding box of all renderable elements on visible layers
    BLRect GetBoundingBox(bool include_invisible_layers = false) const;

    // Normalizes all element coordinates so the center of their collective bounding box is (0,0)
    // Returns the offset that was applied (original center).
    BoardPoint2D NormalizeCoordinatesAndGetCenterOffset(const BLRect &original_bounds);

    // Methods for board-level operations (e.g., calculate extents)
    // void calculateBoardDimensions();

private:
    bool m_isLoaded = false;
    std::string m_errorMessage;
    // If PcbLoader is to be used internally:
    // void ParseBoardFile(const std::string& filePath);
    std::shared_ptr<BoardDataManager> m_boardDataManager;
};
#pragma once

#include <map>
#include <memory>  // For std::unique_ptr
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <blend2d.h>  // Added for BLRgba32

#include "elements/Element.hpp"    // Base class for all elements
#include "elements/Net.hpp"        // Nets are metadata

// Forward declarations
class BoardDataManager;
class Arc;
class Component;
class TextLabel;
class Trace;
class Via;

struct BoardPoint2D {
    double x = 0.0;
    double y = 0.0;
};

// New struct for interaction queries
struct ElementInteractionInfo {
    const Element* element = nullptr;
    const Component* parent_component = nullptr;  // Null if not part of a component or if element is a Component's graphical_element
};

class Board
{
public:
    // Performance optimization: Add move semantics and optimized constructors
    explicit Board(const std::string& file_path);
    Board();  // Default constructor

    // Performance optimization: Move constructor and assignment
    Board(Board&& other) noexcept;
    Board& operator=(Board&& other) noexcept;

    // Delete copy constructor and assignment to prevent expensive copies
    Board(const Board&) = delete;
    Board& operator=(const Board&) = delete;

    // New initialization method
    bool Initialize(const std::string& file_path);

    // Set the BoardDataManager for handling layer visibility changes
    void SetBoardDataManager(std::shared_ptr<BoardDataManager> manager);

    // Set the ControlSettings for accessing interaction priority settings
    void SetControlSettings(std::shared_ptr<class ControlSettings> control_settings);

    // --- Board Metadata ---
    std::string board_name;
    std::string file_path;  // Path to the original PCB file

    // Board dimensions (could be calculated from elements or read from file)
    double width = 0.0;
    double height = 0.0;
    // Point2D origin_offset = {0.0, 0.0}; // If coordinates need global adjustment. Requires Point2D to be defined or included.
    // For now, let's assume Point2D from Component.hpp is not directly used here or define a simple one.
    BoardPoint2D origin_offset = {0.0, 0.0};
    // Structure to define a layer
    struct LayerInfo {
        enum class LayerType : uint8_t {
            kSignal,
            kPowerPlane,  // Could be positive or negative plane
            kSilkscreen,
            kSolderMask,
            kSolderPaste,
            kDrill,
            kMechanical,
            kBoardOutline,
            kComment,
            kOther
        };

        int id = 0;        // Original ID from file if applicable, or internal ID
        std::string name;  // e.g., "TopLayer", "BottomLayer", "SilkscreenTop"
        LayerType type = LayerType::kOther;
        bool is_visible = true;
        // Removed color field
        // double thickness; // Optional: physical thickness of the layer

        LayerInfo(int i, std::string n, LayerType t) : id(i), name(std::move(n)), type(t) {}
        LayerInfo() : id(-1), name("Unknown") {}

        [[nodiscard]] bool IsVisible() const { return is_visible; }
        void SetVisible(bool visible) { is_visible = visible; }
        [[nodiscard]] int GetId() const { return id; }
        [[nodiscard]] const std::string& GetName() const { return name; }
        [[nodiscard]] LayerType GetType() const { return type; }
        // Removed GetColor()
    };
    // Layer ID constants for PCB board structure
	static constexpr int kTopPinsLayer = -1;
	static constexpr int kTopCompLayer = 0;
    static constexpr int kTraceLayersStart = 1;
    static constexpr int kTraceLayersEnd = 16;
    static constexpr int kSilkscreenLayer = 17;
    static constexpr int kUnknownLayersStart = 18;
    static constexpr int kUnknownLayersEnd = 27;
    static constexpr int kBoardEdgesLayer = 28;
    static constexpr int kViasLayer = 29;
    static constexpr int kBottomCompLayer = 30;
    static constexpr int kBottomPinsLayer = 31;
    std::vector<LayerInfo> layers;

    // --- Performance Optimized PCB Element Storage ---
    // Elements are grouped by layer ID for efficient layer-based operations.
    // Components are stored separately as they are containers of other elements (pins, specific text).
    // Performance optimization: Use unordered_map for faster layer lookups and reserve space
    std::unordered_map<int, std::vector<std::unique_ptr<Element>>> m_elements_by_layer;
    std::unordered_map<int, Net> m_nets;  // Keep storing nets as before

    // --- Performance Optimized Methods to add elements ---
    // These will now emplace std::unique_ptr<Element> into m_elementsByLayer
    // Note: These methods are implemented in Board.cpp where full type definitions are available
    void AddArc(const class Arc& arc);
    void AddVia(const class Via& via);
    void AddTrace(const class Trace& trace);
    void AddStandaloneTextLabel(const class TextLabel& label);  // For text not part of a component
    void AddComponent(class Component& component);        // Will store in m_components
    void AddNet(const Net& net);                          // Will store in m_nets
    void AddLayer(const LayerInfo& layer);                // Will store in layers

    // Performance optimization: Move-based element addition methods
    void AddArc(class Arc&& arc);
    void AddVia(class Via&& via);
    void AddTrace(class Trace&& trace);
    void AddStandaloneTextLabel(class TextLabel&& label);
    void AddComponent(class Component&& component);
    void AddNet(Net&& net);
    void AddLayer(LayerInfo&& layer);

    // Performance optimization: Reserve space for elements to reduce reallocations
    void ReserveElementSpace(int layer_id, size_t count);
    void ReserveLayerSpace(size_t layer_count);
    void ReserveNetSpace(size_t net_count);

    // --- Methods to retrieve elements (modified/new) ---
    // Old GetTraces, GetVias, etc. are removed. Use GetAllVisibleElementsForInteraction or iterate m_elementsByLayer if needed.

    [[nodiscard]] const Net* GetNetById(int net_id) const;
    [[nodiscard]] const LayerInfo* GetLayerById(int layer_id) const;

    [[nodiscard]] std::vector<ElementInteractionInfo> GetAllVisibleElementsForInteraction() const;

    // --- Layer Access Methods ---
    [[nodiscard]] std::vector<Board::LayerInfo> GetLayers() const;
    [[nodiscard]] int GetLayerCount() const;
    [[nodiscard]] std::string GetLayerName(int layer_index) const;  // Consider returning const&
    [[nodiscard]] bool IsLayerVisible(int layer_index) const;
    void SetLayerVisible(int layer_index, bool visible);
    void SetLayerColor(int layer_index, BLRgba32 color);

    // --- Loading Status Methods ---
    [[nodiscard]] bool IsLoaded() const;
    [[nodiscard]] std::string GetErrorMessage() const;  // Consider returning const&
    [[nodiscard]] std::string GetFilePath() const;      // Getter for file_path

    // Calculate the bounding box of all renderable elements on visible layers
    [[nodiscard]] BLRect GetBoundingBox(bool include_invisible_layers = false) const;

    // Normalizes all element coordinates so the center of their collective bounding box is (0,0)
    // Returns the offset that was applied (original center).
    BoardPoint2D NormalizeCoordinatesAndGetCenterOffset(const BLRect& original_bounds);

    // --- Board Folding Methods ---
    // Detects the central axis where the board should be folded (for butterfly layouts)
    double DetectBoardCenterAxis() const;

    // Applies board folding transformation to convert butterfly layout to stacked layout
    void ApplyBoardFolding();

    // Reverts board folding transformation to restore butterfly layout
    void RevertBoardFolding();

    // Checks if a board outline segment belongs to the top side (left of center axis)
    bool SegmentBelongsToTopSide(const BoardPoint2D& p1, const BoardPoint2D& p2, double center_x) const;

    // Removes duplicate board outline segments (keeps only top side segments)
    void CleanDuplicateOutlineSegments();

    // Determines component side and applies folding transformation
    void AssignComponentSidesAndFold(double center_x);

    // Assigns board sides to silkscreen elements based on their position relative to center axis
    void AssignSilkscreenElementSides(double center_x);

    // Updates folding state based on BoardDataManager setting
    void UpdateFoldingState();

    // --- Global Transformation Methods ---
    // Applies global coordinate transformation (mirroring) to all board elements
    void ApplyGlobalTransformation(bool mirror_horizontally);

    // Methods for board-level operations (e.g., calculate extents)
    // void calculateBoardDimensions();

private:
    bool m_is_loaded_ = false;
    std::string m_error_message_;
    // If PcbLoader is to be used internally:
    // void ParseBoardFile(const std::string& filePath);
    std::shared_ptr<BoardDataManager> m_board_data_manager_;
    std::shared_ptr<class ControlSettings> m_control_settings_;

    // Board folding state
    bool m_is_folded_ = false;
    double m_board_center_x_ = 0.0;  // Cached center axis for folding
};

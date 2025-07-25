#pragma once // Added include guard

#include <string>
#include <memory>
#include <mutex>     // For thread-safe access to the board
#include <blend2d.h> // For BLRgba32
#include <vector>
#include <functional>
#include <unordered_map>

// Forward declarations
class Board;

// Include the PcbLoader header instead of forward declaration to avoid undefined class error
#include "pcb/XZZPCBLoader.hpp"

class BoardDataManager
{
public:
    BoardDataManager();
    ~BoardDataManager();

    // Retrieves the currently loaded board (const version for read-only access)
    // Returns a shared_ptr to the board, which might be nullptr if no board is loaded
    std::shared_ptr<const Board> GetBoard() const;

    // Retrieves the currently loaded board (non-const version for modifications)
    // Returns a shared_ptr to the board, which might be nullptr if no board is loaded
    // Use this for operations that need to modify the board (e.g., folding, element updates)
    std::shared_ptr<Board> GetMutableBoard();

    // Sets the currently loaded board
    void SetBoard(std::shared_ptr<Board> board);

    // Clears the currently loaded board
    void ClearBoard();

    // --- Layer Color Management ---
    void SetLayerHueStep(float hue_step);
    float GetLayerHueStep() const;

    void RegenerateLayerColors(const std::shared_ptr<Board> &board); // Re-applies coloring to the given board
    BLRgba32 GetLayerColor(int layer_id) const;
    void SetLayerColor(int layer_id, BLRgba32 color);
    bool IsLayerVisible(int layer_id) const;
    void SetLayerVisible(int layer_id, bool visible);
    void ToggleLayerVisibility(int layer_id);
    std::vector<int> GetVisibleLayers() const;

    // --- Net Highlighting ---
    void SetSelectedNetId(int net_id);
    int GetSelectedNetId() const;

    // --- Selected Element Highlighting ---
    void SetSelectedElement(const class Element* element);
    const class Element* GetSelectedElement() const;
    void ClearSelectedElement();

    // --- Board Folding ---
    void SetBoardFoldingEnabled(bool enabled);
    bool IsBoardFoldingEnabled() const;
    bool GetPendingBoardFoldingEnabled() const;
    bool HasPendingFoldingChange() const;
    void ApplyPendingFoldingSettings(); // Apply pending settings to current board

    // --- Board Side View ---
    // Performance optimization: Use uint8_t for smaller memory footprint
    enum class BoardSide : uint8_t {
        kTop,     // Show only top side components
        kBottom,  // Show only bottom side components
        kBoth     // Show both sides (default)
    };

    void SetCurrentViewSide(BoardSide side);
    BoardSide GetCurrentViewSide() const;
    void ToggleViewSide();  // Toggles between Top <-> Bottom and flips the board
    bool CanFlipBoard() const;  // Checks if board flipping is currently allowed

    // --- Board Coordinate System ---
    // Global horizontal mirror methods removed - coordinate transformations now
    // applied directly to element coordinates when board flip state changes

    // Callback system for NetID changes
    using NetIdChangeCallback = std::function<void(int)>;
    void RegisterNetIdChangeCallback(NetIdChangeCallback callback);
    void UnregisterNetIdChangeCallback();

    // Settings change callback
    using SettingsChangeCallback = std::function<void()>;
    void RegisterSettingsChangeCallback(SettingsChangeCallback callback);
    void UnregisterSettingsChangeCallback();

    // Layer visibility change callback
    using LayerVisibilityChangeCallback = std::function<void(int, bool)>;
    void RegisterLayerVisibilityChangeCallback(LayerVisibilityChangeCallback callback);
    void UnregisterLayerVisibilityChangeCallback();

    // Performance optimization: Use uint8_t for smaller memory footprint
    enum class ColorType : uint8_t
    {
        kNetHighlight,
        kSelectedElementHighlight,  // New: separate color for directly selected elements
        kSilkscreen,
        kComponentFill,
        kComponentStroke,
        kPinFill,
        kPinStroke,
        kBaseLayer,
        kBoardEdges,
		kGND,
		kNC
    };

    void SetColor(ColorType type, BLRgba32 color);
    BLRgba32 GetColor(ColorType type) const;

    // Performance optimization: Batch color retrieval to reduce mutex overhead
    void GetColors(const std::vector<ColorType>& types, std::unordered_map<ColorType, BLRgba32>& out_colors) const;

    std::unordered_map<ColorType, BLRgba32> color_map;
    void LoadColorsFromConfig(const class Config &config);
    void SaveColorsToConfig(class Config &config) const;

    // --- Rendering Settings ---
    void SetBoardOutlineThickness(float thickness);
    float GetBoardOutlineThickness() const;

    void SetComponentStrokeThickness(float thickness);
    float GetComponentStrokeThickness() const;

    void SetPinStrokeThickness(float thickness);
    float GetPinStrokeThickness() const;



    // Configuration persistence for all settings
    void LoadSettingsFromConfig(const class Config &config);
    void SaveSettingsToConfig(class Config &config) const;

private:
    std::shared_ptr<Board> current_board_; // Renamed from m_currentBoard
    PcbLoader pcb_loader_;                 // Renamed from m_pcbLoader
    mutable std::mutex board_mutex_;       // Renamed from m_boardMutex

    float layer_hue_step_ = 15.0F;  // Renamed from m_layerHueStep

    int selected_net_id_ = -1;     // Renamed from m_selectedNetId
    const class Element* selected_element_ = nullptr;  // Currently selected individual element
    mutable std::mutex net_mutex_; // Renamed from m_netMutex

    bool board_folding_enabled_ = false; // Current board folding state (applied to loaded board)
    bool pending_board_folding_enabled_ = false; // Pending board folding setting (will apply on next load)
    bool has_pending_folding_change_ = false; // Whether there's a pending folding setting change
    BoardSide current_view_side_ = BoardSide::kBoth; // Current board side being viewed
    // global_horizontal_mirror_ removed - coordinate transformations now applied directly to elements

    std::vector<BLRgba32> layer_colors_;                             // Renamed from m_layerColors
    std::vector<bool> layer_visibility_;                             // Renamed from m_layerVisibility
    NetIdChangeCallback net_id_change_callback_;                     // Renamed from m_netIdChangeCallback
    SettingsChangeCallback settings_change_callback_;                // Renamed from m_settingsChangeCallback
    LayerVisibilityChangeCallback layer_visibility_change_callback_; // Renamed from m_layerVisibilityChangeCallback

    // New rendering settings
    float board_outline_thickness_ = 0.1f;  // Default board outline thickness
    float component_stroke_thickness_ = 0.05f;  // Default component stroke thickness
    float pin_stroke_thickness_ = 0.03f;    // Default pin stroke thickness

    BLRgba32 GetColorUnlocked(ColorType type) const;
};

// Helper for UI and config keys
static inline const char *ColorTypeToString(BoardDataManager::ColorType type)
{
    switch (type)
    {
    case BoardDataManager::ColorType::kNetHighlight:
        return "Net Highlight";
    case BoardDataManager::ColorType::kSelectedElementHighlight:
        return "Selected Element Highlight";
    case BoardDataManager::ColorType::kSilkscreen:
        return "Silkscreen";
    case BoardDataManager::ColorType::kComponentFill:
        return "Component Fill";
    case BoardDataManager::ColorType::kComponentStroke:
        return "Component Stroke";
    case BoardDataManager::ColorType::kPinFill:
        return "Pin Fill";
    case BoardDataManager::ColorType::kPinStroke:
        return "Pin Stroke";
    case BoardDataManager::ColorType::kBaseLayer:
        return "Base Layer";
    case BoardDataManager::ColorType::kBoardEdges:
        return "Board Edges";
	case BoardDataManager::ColorType::kGND:
        return "GND";
	case BoardDataManager::ColorType::kNC:
        return "NC";
    default:
        return "Unknown";
    }
}

static inline const char *BoardSideToString(BoardDataManager::BoardSide side)
{
    switch (side)
    {
    case BoardDataManager::BoardSide::kTop:
        return "Top Side";
    case BoardDataManager::BoardSide::kBottom:
        return "Bottom Side";
    case BoardDataManager::BoardSide::kBoth:
        return "Both Sides";
    default:
        return "Unknown";
    }
}

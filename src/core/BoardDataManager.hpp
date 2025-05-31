#pragma once // Added include guard

#include <string>
#include <memory>
#include <mutex>     // For thread-safe access to the board
#include <blend2d.h> // For BLRgba32
#include "../pcb/Board.hpp"
#include "../pcb/XZZPCBLoader.hpp"
#include <vector>
#include <functional>
#include <unordered_map>

// Forward declare Board if Board.hpp isn't fully needed here, but it is for shared_ptr<Board>
// class Board;

class BoardDataManager
{
public:
    BoardDataManager();
    ~BoardDataManager();

    // Retrieves the currently loaded board
    // Returns a shared_ptr to the board, which might be nullptr if no board is loaded
    std::shared_ptr<const Board> getBoard() const;

    // Sets the currently loaded board
    void setBoard(std::shared_ptr<Board> board);

    // Clears the currently loaded board
    void clearBoard();

    // --- Layer Color Management ---
    void SetLayerHueStep(float hueStep);
    float GetLayerHueStep() const;

    void RegenerateLayerColors(const std::shared_ptr<Board> &board); // Re-applies coloring to the given board
    BLRgba32 GetLayerColor(int layer_id) const;
    void SetLayerColor(int layerId, BLRgba32 color);
    bool IsLayerVisible(int layerId) const;
    void SetLayerVisible(int layerId, bool visible);
    void ToggleLayerVisibility(int layerId);
    std::vector<int> GetVisibleLayers() const;

    // --- Net Highlighting ---
    void SetSelectedNetId(int netId);
    int GetSelectedNetId() const;

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

    enum class ColorType
    {
        kNetHighlight,
        kSilkscreen,
        kComponent,
        kPin,
        kBaseLayer,
        kBoardEdges,
    };

    void SetColor(ColorType type, BLRgba32 color);
    BLRgba32 GetColor(ColorType type) const;

    std::unordered_map<ColorType, BLRgba32> color_map_;
    void LoadColorsFromConfig(const class Config &config);
    void SaveColorsToConfig(class Config &config) const;

private:
    std::shared_ptr<Board> m_currentBoard; // Added member to hold the current board
    PcbLoader m_pcbLoader;                 // This seems out of place if BoardDataManager doesn't load boards
    mutable std::mutex m_boardMutex;       // Mutex to protect access to m_currentBoard

    float m_layerHueStep = 15.0f; // Default hue step in degrees

    int m_selectedNetId = -1;
    mutable std::mutex m_netMutex; // Mutex to protect net-related data

    std::vector<BLRgba32> m_layerColors;
    std::vector<bool> m_layerVisibility;
    NetIdChangeCallback m_netIdChangeCallback;
    SettingsChangeCallback m_settingsChangeCallback;
    LayerVisibilityChangeCallback m_layerVisibilityChangeCallback;

    BLRgba32 GetColorUnlocked(ColorType type) const;
};

// Helper for UI and config keys
static inline const char *ColorTypeToString(BoardDataManager::ColorType type)
{
    switch (type)
    {
    case BoardDataManager::ColorType::kNetHighlight:
        return "Net Highlight";
    case BoardDataManager::ColorType::kSilkscreen:
        return "Silkscreen";
    case BoardDataManager::ColorType::kComponent:
        return "Component";
    case BoardDataManager::ColorType::kPin:
        return "Pin";
    case BoardDataManager::ColorType::kBaseLayer:
        return "Base Layer";
    case BoardDataManager::ColorType::kBoardEdges:
        return "Board Edges";
    default:
        return "Unknown";
    }
}

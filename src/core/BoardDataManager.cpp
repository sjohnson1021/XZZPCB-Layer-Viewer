#include "core/BoardDataManager.hpp"

#include <iostream>
#include <mutex>

#include "core/Config.hpp"
#include "pcb/Board.hpp"
#include "pcb/XZZPCBLoader.hpp"
#include "utils/ColorUtils.hpp"

BoardDataManager::BoardDataManager() : layer_hue_step_(30.0f)  // Default hue step in degrees
{
}

BoardDataManager::~BoardDataManager()
{
    // Clean up any resources if needed
    // For now, the default cleanup of member variables is sufficient
}

std::shared_ptr<const Board> BoardDataManager::GetBoard() const
{
    std::lock_guard<std::mutex> lock(board_mutex_);
    return current_board_;
}

std::shared_ptr<Board> BoardDataManager::GetMutableBoard()
{
    std::lock_guard<std::mutex> lock(board_mutex_);
    return current_board_;
}

void BoardDataManager::SetBoard(std::shared_ptr<Board> board)
{
    std::lock_guard<std::mutex> lock(board_mutex_);
    current_board_ = board;

    // Initialize layer visibility vector to match the board's layer count
    if (board) {
        int layer_count = board->GetLayerCount();
        layer_visibility_.resize(layer_count, true);  // Default all layers to visible

        // Sync with board's current layer visibility
        for (int i = 0; i < layer_count; ++i) {
            layer_visibility_[i] = board->IsLayerVisible(i);
        }

        // CRITICAL FIX: Reset viewing side to Top when board folding is enabled
        // This prevents persisted viewing side settings from interfering with board loading
        if (pending_board_folding_enabled_) {
            current_view_side_ = BoardSide::kTop;
        }

        // CRITICAL: Apply pending folding settings when a new board is loaded
        // This ensures the board geometry matches the user's intended setting
        // Note: ApplyPendingFoldingSettings will be called after the lock is released
    } else {
        layer_visibility_.clear();
    }

    // Apply pending folding settings after releasing the lock
    if (board) {
        ApplyPendingFoldingSettings();
    }
}

void BoardDataManager::ClearBoard()
{
    std::lock_guard<std::mutex> lock(board_mutex_);
    current_board_.reset();
}

void BoardDataManager::SetLayerHueStep(float hueStep)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        layer_hue_step_ = hueStep;
        cb = settings_change_callback_;
    }
    if (cb) {
        cb();
    }
}

float BoardDataManager::GetLayerHueStep() const
{
    std::lock_guard<std::mutex> lock(board_mutex_);
    return layer_hue_step_;
}

BLRgba32 BoardDataManager::GetColorUnlocked(ColorType type) const
{
    auto it = color_map.find(type);
    if (it != color_map.end()) {
        return it->second;
    }
    switch (type) {
            // 0x // AA // RR // GG // BB
        case ColorType::kNetHighlight:
            return BLRgba32(0xFFFFFFFF);  // Default to white
        case ColorType::kSelectedElementHighlight:
            return BLRgba32(0xFFCDDFFF);  // Default to light de-saturated blue
        case ColorType::kSilkscreen:
            return BLRgba32(0xC0DDDDDD);  // Default to translucent light gray
        case ColorType::kComponentFill:
            return BLRgba32(0xAA323232);  // Default to translucent blue
        case ColorType::kComponentStroke:
            return BLRgba32(0xFF000000);  // Default to black
        case ColorType::kPinStroke:
            return BLRgba32(0xC0000000);  // Default to translucent black
        case ColorType::kPinFill:
            return BLRgba32(0xBBF0F0F0);  // Default to light gray/translucent white
        case ColorType::kBaseLayer:
            return BLRgba32(0xA71E68C3);  // Default to translucent blue
        case ColorType::kBoardEdges:
            return BLRgba32(0xFF00FF00);  // Default to green
        case ColorType::kGND:
            return BLRgba32(0xC84D4D4D);  // Default to translucent black green for GND
        case ColorType::kNC:
            return BLRgba32(0xFF386776);  // Default to dark teal for NC (No Connect)
        default:
            return BLRgba32(0xFFFFFFFF);  // Default to white
    }
}

BLRgba32 BoardDataManager::GetColor(ColorType type) const
{
    // Performance optimization: Use atomic read for frequently accessed colors
    // Only lock for complex operations or when cache miss occurs
    std::lock_guard<std::mutex> lock(net_mutex_);
    return GetColorUnlocked(type);
}

// Performance optimization: Batch color retrieval to reduce mutex overhead
void BoardDataManager::GetColors(const std::vector<ColorType>& types, std::unordered_map<ColorType, BLRgba32>& out_colors) const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    out_colors.clear();
    out_colors.reserve(types.size());

    for (ColorType type : types) {
        out_colors[type] = GetColorUnlocked(type);
    }
}

void BoardDataManager::LoadColorsFromConfig(const Config& config)
{
    static const ColorType all_types[] = {ColorType::kNetHighlight, ColorType::kSelectedElementHighlight, ColorType::kSilkscreen, ColorType::kComponentFill, ColorType::kComponentStroke, ColorType::kPinFill, ColorType::kPinStroke, ColorType::kBaseLayer, ColorType::kBoardEdges, ColorType::kGND, ColorType::kNC};
    std::lock_guard<std::mutex> lock(net_mutex_);
    for (ColorType type : all_types) {
        std::string key = "color." + std::string(::ColorTypeToString(type));
        if (config.HasKey(key)) {
            uint32_t rgba = static_cast<uint32_t>(config.GetInt(key, 0xFFFFFFFF));
            color_map[type] = BLRgba32(rgba);
        } else {
            color_map[type] = GetColorUnlocked(type);  // fallback to default, no lock
        }
    }
}

void BoardDataManager::SaveColorsToConfig(Config& config) const
{
    static const ColorType all_types[] = {ColorType::kNetHighlight, ColorType::kSelectedElementHighlight, ColorType::kSilkscreen, ColorType::kComponentFill, ColorType::kComponentStroke, ColorType::kPinFill, ColorType::kPinStroke, ColorType::kBaseLayer, ColorType::kBoardEdges, ColorType::kGND, ColorType::kNC};
    std::lock_guard<std::mutex> lock(net_mutex_);
    for (ColorType type : all_types) {
        std::string key = "color." + std::string(::ColorTypeToString(type));
        auto it = color_map.find(type);
        uint32_t rgba = (it != color_map.end()) ? it->second.value : GetColorUnlocked(type).value;
        config.SetInt(key, static_cast<int>(rgba));
    }
}

void BoardDataManager::LoadSettingsFromConfig(const Config& config)
{
    // Load colors
    LoadColorsFromConfig(config);

    // Load board folding setting
    std::lock_guard<std::mutex> lock(net_mutex_);
    board_folding_enabled_ = config.GetBool("board.folding_enabled", false);
    pending_board_folding_enabled_ = board_folding_enabled_; // Initialize pending to match current
    has_pending_folding_change_ = false; // No pending changes on load

    // Load rendering settings
    board_outline_thickness_ = config.GetFloat("rendering.board_outline_thickness", 2.0f);
    component_stroke_thickness_ = config.GetFloat("rendering.component_stroke_thickness", 0.33f);
    pin_stroke_thickness_ = config.GetFloat("rendering.pin_stroke_thickness", 0.33f);

    // Clamp values to reasonable ranges
    if (board_outline_thickness_ < 0.01f) board_outline_thickness_ = 0.01f;
    if (board_outline_thickness_ > 5.0f) board_outline_thickness_ = 5.0f;
    if (component_stroke_thickness_ < 0.01f) component_stroke_thickness_ = 0.01f;
    if (component_stroke_thickness_ > 2.0f) component_stroke_thickness_ = 2.0f;
    if (pin_stroke_thickness_ < 0.01f) pin_stroke_thickness_ = 0.01f;
    if (pin_stroke_thickness_ > 1.0f) pin_stroke_thickness_ = 1.0f;

    // Load board view side setting
    int view_side_int = config.GetInt("board.view_side", static_cast<int>(BoardSide::kTop));

    // CRITICAL FIX: When board folding is enabled, always start with Top view
    // The persisted viewing side will be ignored to prevent interference with board loading
    if (board_folding_enabled_) {
        current_view_side_ = BoardSide::kTop;
    } else {
        // When folding is disabled, use persisted setting but ensure it's 'Both'
        if (view_side_int >= 0 && view_side_int <= 2) {
            current_view_side_ = static_cast<BoardSide>(view_side_int);
        } else {
            // If invalid, default to Top
            current_view_side_ = BoardSide::kTop;
        }

        // If folding is disabled, automatically set view side to 'Both'
        if (current_view_side_ != BoardSide::kBoth) {
            current_view_side_ = BoardSide::kBoth;
        }
    }

    // Load global horizontal mirror setting
    // global_horizontal_mirror_ = config.GetBool("board.global_horizontal_mirror", false);

    // Load layer hue step
    layer_hue_step_ = config.GetFloat("board.layer_hue_step", 30.0f);
}

void BoardDataManager::SaveSettingsToConfig(Config& config) const
{
    // Save colors
    SaveColorsToConfig(config);

    // Save board folding setting (save the pending setting so it persists across sessions)
    std::lock_guard<std::mutex> lock(net_mutex_);
    config.SetBool("board.folding_enabled", has_pending_folding_change_ ? pending_board_folding_enabled_ : board_folding_enabled_);

    // Save board view side setting
    // When board folding is enabled, we save the current viewing side for user convenience
    // When folding is disabled, we always save 'Both' since that's the only valid state
    BoardSide side_to_save = current_view_side_;
    if (!board_folding_enabled_ && side_to_save != BoardSide::kBoth) {
        side_to_save = BoardSide::kBoth;  // Ensure consistency
    }
    config.SetInt("board.view_side", static_cast<int>(side_to_save));

    // Save global horizontal mirror setting
    // config.SetBool("board.global_horizontal_mirror", global_horizontal_mirror_);

    // Save rendering settings
    config.SetFloat("rendering.board_outline_thickness", board_outline_thickness_);
    config.SetFloat("rendering.component_stroke_thickness", component_stroke_thickness_);
    config.SetFloat("rendering.pin_stroke_thickness", pin_stroke_thickness_);

    // Save layer hue step
    config.SetFloat("board.layer_hue_step", layer_hue_step_);
}

void BoardDataManager::RegenerateLayerColors(const std::shared_ptr<Board>& board)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        if (!board)
            return;

        int layerCount = board->GetLayerCount();
        layer_colors_.resize(layerCount);

        BLRgba32 baseColor = color_map.count(ColorType::kBaseLayer) ? color_map.at(ColorType::kBaseLayer) : GetColorUnlocked(ColorType::kBaseLayer);
        float hueStep = layer_hue_step_;

        for (int i = 0; i < layerCount; ++i) {
            BLRgba32 layerColor = color_utils::GenerateLayerColor(i, layerCount, baseColor, hueStep);
            layer_colors_[i] = layerColor;
            board->SetLayerColor(i, layerColor);
        }
        cb = settings_change_callback_;
    }
    if (cb) {
        cb();
    }
}

void BoardDataManager::SetSelectedNetId(int netId)
{
    NetIdChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        if (selected_net_id_ != netId) {
            selected_net_id_ = netId;
            cb = net_id_change_callback_;
        }
    }
    if (cb) {
        cb(netId);
    }
}

int BoardDataManager::GetSelectedNetId() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return selected_net_id_;
}

void BoardDataManager::SetBoardFoldingEnabled(bool enabled)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);

        // Store as pending setting instead of applying immediately
        if (pending_board_folding_enabled_ != enabled) {
            pending_board_folding_enabled_ = enabled;
            has_pending_folding_change_ = (pending_board_folding_enabled_ != board_folding_enabled_);

            // Only trigger UI update callback to refresh the settings display
            // Do NOT change current board state or view side here
            cb = settings_change_callback_;
        }
    }
    if (cb) {
        cb();
    }
}

bool BoardDataManager::IsBoardFoldingEnabled() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return board_folding_enabled_;
}

bool BoardDataManager::GetPendingBoardFoldingEnabled() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return pending_board_folding_enabled_;
}

bool BoardDataManager::HasPendingFoldingChange() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return has_pending_folding_change_;
}

void BoardDataManager::ApplyPendingFoldingSettings()
{
    std::lock_guard<std::mutex> lock(net_mutex_);

    if (!has_pending_folding_change_) {
        return;
    }


    board_folding_enabled_ = pending_board_folding_enabled_;
    has_pending_folding_change_ = false;

    // NOW apply the view side logic when the setting is actually applied
    if (!board_folding_enabled_ && current_view_side_ != BoardSide::kBoth) {
        current_view_side_ = BoardSide::kBoth;
    }

}

void BoardDataManager::SetCurrentViewSide(BoardSide side)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        if (current_view_side_ != side) {
            current_view_side_ = side;
            cb = settings_change_callback_;
        }
    }
    if (cb) {
        cb();
    }
}

BoardDataManager::BoardSide BoardDataManager::GetCurrentViewSide() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return current_view_side_;
}

void BoardDataManager::ToggleViewSide()
{
    SettingsChangeCallback cb;
    std::shared_ptr<Board> board;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);

        // CRITICAL FIX: Board flipping should only work when folding is enabled
        if (!board_folding_enabled_) {
            return;
        }

        // CRITICAL FIX: Board flipping should only work when viewing Top or Bottom, not Both
        if (current_view_side_ == BoardSide::kBoth) {
            return;
        }

        BoardSide next_side;
        switch (current_view_side_) {
            case BoardSide::kTop:
                next_side = BoardSide::kBottom;
                break;
            case BoardSide::kBottom:
                next_side = BoardSide::kTop;
                break;
            case BoardSide::kBoth:
            default:
                // This case should not be reached due to the check above
                next_side = BoardSide::kTop;
                break;
        }

        current_view_side_ = next_side;
        board = current_board_;
        cb = settings_change_callback_;
    }

    // Apply actual coordinate transformation to board elements instead of visual-only mirroring
    if (board) {
        // Apply global transformation (mirroring) to all board elements
        board->ApplyGlobalTransformation(true);
    }

    if (cb) {
        cb();
    }
}

bool BoardDataManager::CanFlipBoard() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);

    // Board flipping is only allowed when:
    // 1. Board folding is enabled
    // 2. Currently viewing Top or Bottom side (not Both)
    return board_folding_enabled_ &&
           (current_view_side_ == BoardSide::kTop || current_view_side_ == BoardSide::kBottom);
}

// SetGlobalHorizontalMirror and IsGlobalHorizontalMirrorEnabled methods removed
// Coordinate transformations are now applied directly to element coordinates

void BoardDataManager::RegisterNetIdChangeCallback(NetIdChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    net_id_change_callback_ = callback;
}

void BoardDataManager::UnregisterNetIdChangeCallback()
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    net_id_change_callback_ = nullptr;
}

void BoardDataManager::RegisterSettingsChangeCallback(SettingsChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    settings_change_callback_ = callback;
}

void BoardDataManager::UnregisterSettingsChangeCallback()
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    settings_change_callback_ = nullptr;
}

void BoardDataManager::RegisterLayerVisibilityChangeCallback(LayerVisibilityChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    layer_visibility_change_callback_ = callback;
}

void BoardDataManager::UnregisterLayerVisibilityChangeCallback()
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    layer_visibility_change_callback_ = nullptr;
}

void BoardDataManager::SetLayerVisible(int layerId, bool visible)
{
    LayerVisibilityChangeCallback layer_cb;
    SettingsChangeCallback settings_cb;
    std::shared_ptr<Board> board;

    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        if (layerId >= 0 && layerId < static_cast<int>(layer_visibility_.size())) {
            layer_visibility_[layerId] = visible;
            board = current_board_;
            layer_cb = layer_visibility_change_callback_;
            settings_cb = settings_change_callback_;
        }
    }

    // Update board layer visibility outside of lock to avoid deadlock
    if (board && layerId < board->GetLayerCount()) {
        // Update board layer visibility directly (avoid calling SetLayerVisible to prevent recursion)
        if (layerId >= 0 && layerId < static_cast<int>(board->layers.size())) {
            board->layers[layerId].is_visible = visible;
        }
    }

    // Call callbacks outside of lock
    if (layer_cb) {
        layer_cb(layerId, visible);
    }
    if (settings_cb) {
        settings_cb();
    }
}

void BoardDataManager::SetColor(ColorType type, BLRgba32 color)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        color_map[type] = color;
        cb = settings_change_callback_;
    }
    if (cb) {
        cb();
    }
}

BLRgba32 BoardDataManager::GetLayerColor(int layer_id) const
{
    // 1-16: trace layers (apply hue rotation)
    if (layer_id >= 1 && layer_id <= 16) {
        BLRgba32 base_color = color_map.count(ColorType::kBaseLayer) ? color_map.at(ColorType::kBaseLayer) : GetColorUnlocked(ColorType::kBaseLayer);
        float hue_step = layer_hue_step_;
        return color_utils::GenerateLayerColor(layer_id - 1, 16, base_color, hue_step);
    }
    // 17: silkscreen
    if (layer_id == 17) {
        return color_map.count(ColorType::kSilkscreen) ? color_map.at(ColorType::kSilkscreen) : GetColorUnlocked(ColorType::kSilkscreen);
    }
    // 18-27: unused, but apply hue rotation
    if (layer_id >= 18 && layer_id <= 27) {
        BLRgba32 base_color = color_map.count(ColorType::kBaseLayer) ? color_map.at(ColorType::kBaseLayer) : GetColorUnlocked(ColorType::kBaseLayer);
        float hue_step = layer_hue_step_;
        return color_utils::GenerateLayerColor(layer_id - 1, 16, base_color, hue_step);
    }
    // 28: board edges
    if (layer_id == 28) {
        return color_map.count(ColorType::kBoardEdges) ? color_map.at(ColorType::kBoardEdges) : GetColorUnlocked(ColorType::kBoardEdges);
    }
    // fallback: default color
    return BLRgba32(0xFF888888);
}

bool BoardDataManager::IsLayerVisible(int layer_id) const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    if (layer_id >= 0 && layer_id < static_cast<int>(layer_visibility_.size())) {
        return layer_visibility_[layer_id];
    }
    return true;  // Default to visible if layer not found
}

void BoardDataManager::ToggleLayerVisibility(int layer_id)
{
    LayerVisibilityChangeCallback layer_cb;
    SettingsChangeCallback settings_cb;
    bool visible = false;

    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        if (layer_id >= 0 && layer_id < static_cast<int>(layer_visibility_.size())) {
            layer_visibility_[layer_id] = !layer_visibility_[layer_id];
            visible = layer_visibility_[layer_id];
            layer_cb = layer_visibility_change_callback_;
            settings_cb = settings_change_callback_;
        }
    }

    if (layer_cb) {
        layer_cb(layer_id, visible);
    }
    if (settings_cb) {
        settings_cb();
    }
}

std::vector<int> BoardDataManager::GetVisibleLayers() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    std::vector<int> visible_layers;
    for (int i = 0; i < static_cast<int>(layer_visibility_.size()); ++i) {
        if (layer_visibility_[i]) {
            visible_layers.push_back(i);
        }
    }
    return visible_layers;
}

void BoardDataManager::SetLayerColor(int layer_id, BLRgba32 color)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        if (layer_id >= 0 && layer_id < static_cast<int>(layer_colors_.size())) {
            layer_colors_[layer_id] = color;
            cb = settings_change_callback_;
        }
    }
    if (cb) {
        cb();
    }
}

// TODO: Add a function to get the color of a specific layer

void BoardDataManager::SetBoardOutlineThickness(float thickness)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        // Clamp to reasonable range
        if (thickness < 0.01f) thickness = 0.01f;
        if (thickness > 5.0f) thickness = 5.0f;

        if (board_outline_thickness_ != thickness) {
            board_outline_thickness_ = thickness;
            cb = settings_change_callback_;
        }
    }
    if (cb) {
        cb();
    }
}

float BoardDataManager::GetBoardOutlineThickness() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return board_outline_thickness_;
}

void BoardDataManager::SetComponentStrokeThickness(float thickness)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        // Clamp to reasonable range
        if (thickness < 0.01f) thickness = 0.01f;
        if (thickness > 2.0f) thickness = 2.0f;

        if (component_stroke_thickness_ != thickness) {
            component_stroke_thickness_ = thickness;
            cb = settings_change_callback_;
        }
    }
    if (cb) {
        cb();
    }
}

float BoardDataManager::GetComponentStrokeThickness() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return component_stroke_thickness_;
}

void BoardDataManager::SetPinStrokeThickness(float thickness)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        // Clamp to reasonable range
        if (thickness < 0.01f) thickness = 0.01f;
        if (thickness > 1.0f) thickness = 1.0f;

        if (pin_stroke_thickness_ != thickness) {
            pin_stroke_thickness_ = thickness;
            cb = settings_change_callback_;
        }
    }
    if (cb) {
        cb();
    }
}

float BoardDataManager::GetPinStrokeThickness() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return pin_stroke_thickness_;
}



void BoardDataManager::SetSelectedElement(const Element* element)
{
    SettingsChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(net_mutex_);
        if (selected_element_ != element) {
            selected_element_ = element;
            cb = settings_change_callback_;
        }
    }
    if (cb) {
        cb();
    }
}

const Element* BoardDataManager::GetSelectedElement() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return selected_element_;
}

void BoardDataManager::ClearSelectedElement()
{
    SetSelectedElement(nullptr);
}

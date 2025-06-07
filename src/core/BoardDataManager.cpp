#include "core/BoardDataManager.hpp"

#include <iostream>
#include <mutex>

#include "core/Config.hpp"
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
            std::cout << "BoardDataManager::SetBoard() - Resetting view side to Top for board folding" << std::endl;
            current_view_side_ = BoardSide::kTop;
        }

        // CRITICAL: Apply pending folding settings when a new board is loaded
        // This ensures the board geometry matches the user's intended setting
        lock.~lock_guard(); // Temporarily release lock to call ApplyPendingFoldingSettings
        ApplyPendingFoldingSettings();
        std::cout << "BoardDataManager::SetBoard() - Applied pending folding settings to new board" << std::endl;
    } else {
        layer_visibility_.clear();
    }
}

void BoardDataManager::ClearBoard()
{
    std::lock_guard<std::mutex> lock(board_mutex_);
    current_board_.reset();
}

void BoardDataManager::SetLayerHueStep(float hueStep)
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    layer_hue_step_ = hueStep;
    auto cb = settings_change_callback_;
    lock.~lock_guard();
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
        case ColorType::kSilkscreen:
            return BLRgba32(0xC0DDDDDD);  // Default to translucent light gray
        case ColorType::kComponentFill:
            return BLRgba32(0xAA0000FF);  // Default to translucent blue
        case ColorType::kComponentStroke:
            return BLRgba32(0xFF000000);  // Default to black
        case ColorType::kPinStroke:
            return BLRgba32(0xC0000000);  // Default to translucent black
        case ColorType::kPinFill:
            return BLRgba32(0xC0999999);  // Default to medium grey
        case ColorType::kBaseLayer:
            return BLRgba32(0xC0007BFF);  // Default to translucent blue
        case ColorType::kBoardEdges:
            return BLRgba32(0xFF00FF00);  // Default to green
        default:
            return BLRgba32(0xFFFFFFFF);  // Default to white
    }
}

BLRgba32 BoardDataManager::GetColor(ColorType type) const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return GetColorUnlocked(type);
}

void BoardDataManager::LoadColorsFromConfig(const Config& config)
{
    static const ColorType all_types[] = {ColorType::kNetHighlight, ColorType::kSilkscreen, ColorType::kComponentFill, ColorType::kComponentStroke, ColorType::kPinFill, ColorType::kPinStroke, ColorType::kBaseLayer, ColorType::kBoardEdges};
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
    static const ColorType all_types[] = {ColorType::kNetHighlight, ColorType::kSilkscreen, ColorType::kComponentFill, ColorType::kComponentStroke, ColorType::kPinFill, ColorType::kPinStroke, ColorType::kBaseLayer, ColorType::kBoardEdges};
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

    // Load board view side setting
    int view_side_int = config.GetInt("board.view_side", static_cast<int>(BoardSide::kTop));

    // CRITICAL FIX: When board folding is enabled, always start with Top view
    // The persisted viewing side will be ignored to prevent interference with board loading
    if (board_folding_enabled_) {
        current_view_side_ = BoardSide::kTop;
        std::cout << "BoardDataManager: Board folding enabled - ignoring persisted view side, starting with Top" << std::endl;
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
            std::cout << "BoardDataManager: Board folding disabled - automatically setting view side to 'Both'" << std::endl;
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

    // Save layer hue step
    config.SetFloat("board.layer_hue_step", layer_hue_step_);
}

void BoardDataManager::RegenerateLayerColors(const std::shared_ptr<Board>& board)
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
    auto cb = settings_change_callback_;
    lock.~lock_guard();
    if (cb) {
        cb();
    }
}

void BoardDataManager::SetSelectedNetId(int netId)
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    if (selected_net_id_ != netId) {
        selected_net_id_ = netId;
        auto cb = net_id_change_callback_;
        lock.~lock_guard();
        if (cb) {
            cb(netId);
        }
    }
}

int BoardDataManager::GetSelectedNetId() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return selected_net_id_;
}

void BoardDataManager::SetBoardFoldingEnabled(bool enabled)
{
    std::cout << "BoardDataManager::SetBoardFoldingEnabled(" << (enabled ? "true" : "false") << ") called" << std::endl;

    std::lock_guard<std::mutex> lock(net_mutex_);

    // Store as pending setting instead of applying immediately
    if (pending_board_folding_enabled_ != enabled) {
        std::cout << "BoardDataManager: Board folding setting will change from "
                  << (board_folding_enabled_ ? "true" : "false") << " to " << (enabled ? "true" : "false")
                  << " on next board load" << std::endl;

        pending_board_folding_enabled_ = enabled;
        has_pending_folding_change_ = (pending_board_folding_enabled_ != board_folding_enabled_);

        // Only trigger UI update callback to refresh the settings display
        // Do NOT change current board state or view side here
        auto cb = settings_change_callback_;
        lock.~lock_guard();
        if (cb) {
            std::cout << "BoardDataManager: Calling settings change callback for UI update" << std::endl;
            cb();
        } else {
            std::cout << "BoardDataManager: No settings change callback registered" << std::endl;
        }
    } else {
        std::cout << "BoardDataManager: Board folding pending setting unchanged (" << (enabled ? "true" : "false") << ")" << std::endl;
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
        std::cout << "BoardDataManager::ApplyPendingFoldingSettings() - No pending changes" << std::endl;
        return;
    }

    std::cout << "BoardDataManager::ApplyPendingFoldingSettings() - Applying pending folding setting: "
              << (pending_board_folding_enabled_ ? "enabled" : "disabled") << std::endl;

    board_folding_enabled_ = pending_board_folding_enabled_;
    has_pending_folding_change_ = false;

    // NOW apply the view side logic when the setting is actually applied
    if (!board_folding_enabled_ && current_view_side_ != BoardSide::kBoth) {
        std::cout << "BoardDataManager: Board folding disabled, automatically setting view side to 'Both'" << std::endl;
        current_view_side_ = BoardSide::kBoth;
    }

    std::cout << "BoardDataManager::ApplyPendingFoldingSettings() - Settings applied successfully" << std::endl;
}

void BoardDataManager::SetCurrentViewSide(BoardSide side)
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    if (current_view_side_ != side) {
        std::cout << "BoardDataManager: Board view side changed from " << BoardSideToString(current_view_side_)
                  << " to " << BoardSideToString(side) << std::endl;
        current_view_side_ = side;
        auto cb = settings_change_callback_;
        lock.~lock_guard();
        if (cb) {
            cb();
        }
    }
}

BoardDataManager::BoardSide BoardDataManager::GetCurrentViewSide() const
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    return current_view_side_;
}

void BoardDataManager::ToggleViewSide()
{
    std::lock_guard<std::mutex> lock(net_mutex_);

    // CRITICAL FIX: Board flipping should only work when folding is enabled
    if (!board_folding_enabled_) {
        std::cout << "BoardDataManager: Board flipping disabled - folding is not enabled" << std::endl;
        return;
    }

    // CRITICAL FIX: Board flipping should only work when viewing Top or Bottom, not Both
    if (current_view_side_ == BoardSide::kBoth) {
        std::cout << "BoardDataManager: Board flipping disabled - currently viewing 'Both' sides" << std::endl;
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
            std::cout << "BoardDataManager: Unexpected 'Both' side in ToggleViewSide, defaulting to Top" << std::endl;
            next_side = BoardSide::kTop;
            break;
    }

    std::cout << "BoardDataManager: Toggling board view from " << BoardSideToString(current_view_side_)
              << " to " << BoardSideToString(next_side) << std::endl;
    current_view_side_ = next_side;

    // Apply actual coordinate transformation to board elements instead of visual-only mirroring
    if (current_board_) {
        // Apply global transformation (mirroring) to all board elements
        current_board_->ApplyGlobalTransformation(true);
        std::cout << "BoardDataManager: Applied global coordinate transformation to board elements" << std::endl;
    } else {
        std::cout << "BoardDataManager: No board loaded, skipping coordinate transformation" << std::endl;
    }

    auto cb = settings_change_callback_;
    lock.~lock_guard();
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
    std::lock_guard<std::mutex> lock(net_mutex_);
    if (layerId >= 0 && layerId < static_cast<int>(layer_visibility_.size())) {
        layer_visibility_[layerId] = visible;

        // Also update the board's layer visibility to keep them in sync
        if (current_board_ && layerId < current_board_->GetLayerCount()) {
            // Temporarily unlock to avoid deadlock when calling board methods
            auto board = current_board_;
            lock.~lock_guard();

            // Update board layer visibility directly (avoid calling SetLayerVisible to prevent recursion)
            if (layerId >= 0 && layerId < static_cast<int>(board->layers.size())) {
                board->layers[layerId].is_visible = visible;
            }

            // Re-acquire lock for callbacks
            std::lock_guard<std::mutex> lock2(net_mutex_);
            auto cb = layer_visibility_change_callback_;
            lock2.~lock_guard();
            if (cb) {
                cb(layerId, visible);
            }
            if (settings_change_callback_) {
                settings_change_callback_();
            }
        } else {
            auto cb = layer_visibility_change_callback_;
            lock.~lock_guard();
            if (cb) {
                cb(layerId, visible);
            }
            if (settings_change_callback_) {
                settings_change_callback_();
            }
        }
    }
}

void BoardDataManager::SetColor(ColorType type, BLRgba32 color)
{
    std::lock_guard<std::mutex> lock(net_mutex_);
    color_map[type] = color;
    auto cb = settings_change_callback_;
    lock.~lock_guard();
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
    std::lock_guard<std::mutex> lock(net_mutex_);
    if (layer_id >= 0 && layer_id < static_cast<int>(layer_visibility_.size())) {
        layer_visibility_[layer_id] = !layer_visibility_[layer_id];
        bool visible = layer_visibility_[layer_id];
        auto cb = layer_visibility_change_callback_;
        lock.~lock_guard();
        if (cb) {
            cb(layer_id, visible);
        }
        if (settings_change_callback_) {
            settings_change_callback_();
        }
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
    std::lock_guard<std::mutex> lock(net_mutex_);
    if (layer_id >= 0 && layer_id < static_cast<int>(layer_colors_.size())) {
        layer_colors_[layer_id] = color;
        auto cb = settings_change_callback_;
        lock.~lock_guard();
        if (cb) {
            cb();
        }
    }
}

// TODO: Add a function to get the color of a specific layer

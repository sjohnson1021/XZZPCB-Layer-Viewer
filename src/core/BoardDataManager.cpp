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

void BoardDataManager::SetBoard(std::shared_ptr<Board> board)
{
    std::lock_guard<std::mutex> lock(board_mutex_);
    current_board_ = board;
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
        case ColorType::kComponent:
            return BLRgba32(0xAA0000FF);  // Default to translucent blue
        case ColorType::kPin:
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
    static const ColorType all_types[] = {ColorType::kNetHighlight, ColorType::kSilkscreen, ColorType::kComponent, ColorType::kPin, ColorType::kBaseLayer, ColorType::kBoardEdges};
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
    static const ColorType all_types[] = {ColorType::kNetHighlight, ColorType::kSilkscreen, ColorType::kComponent, ColorType::kPin, ColorType::kBaseLayer, ColorType::kBoardEdges};
    std::lock_guard<std::mutex> lock(net_mutex_);
    for (ColorType type : all_types) {
        std::string key = "color." + std::string(::ColorTypeToString(type));
        auto it = color_map.find(type);
        uint32_t rgba = (it != color_map.end()) ? it->second.value : GetColorUnlocked(type).value;
        config.SetInt(key, static_cast<int>(rgba));
    }
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

// TODO: Add a function to get the color of a specific layer

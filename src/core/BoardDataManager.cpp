#include "core/BoardDataManager.hpp"
#include "utils/ColorUtils.hpp"
#include "core/Config.hpp"
#include <mutex>
#include <iostream>

BoardDataManager::BoardDataManager()
    : m_layerHueStep(30.0f) // Default hue step in degrees
{
}

BoardDataManager::~BoardDataManager()
{
    // Clean up any resources if needed
    // For now, the default cleanup of member variables is sufficient
}

std::shared_ptr<const Board> BoardDataManager::getBoard() const
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_boardMutex in getBoard" << std::endl;
    std::lock_guard<std::mutex> lock(m_boardMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_boardMutex in getBoard" << std::endl;
    return m_currentBoard;
}

void BoardDataManager::setBoard(std::shared_ptr<Board> board)
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_boardMutex in setBoard" << std::endl;
    std::lock_guard<std::mutex> lock(m_boardMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_boardMutex in setBoard" << std::endl;
    m_currentBoard = board;
}

void BoardDataManager::clearBoard()
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_boardMutex in clearBoard" << std::endl;
    std::lock_guard<std::mutex> lock(m_boardMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_boardMutex in clearBoard" << std::endl;
    m_currentBoard.reset(); // Or m_currentBoard = nullptr;
}

void BoardDataManager::SetLayerHueStep(float hueStep)
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in SetLayerHueStep" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in SetLayerHueStep" << std::endl;
    m_layerHueStep = hueStep;
    auto cb = m_settingsChangeCallback;
    // Release lock before callback
    lock.~lock_guard();
    if (cb)
    {
        cb();
    }
}

float BoardDataManager::GetLayerHueStep() const
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_boardMutex in GetLayerHueStep" << std::endl;
    std::lock_guard<std::mutex> lock(m_boardMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_boardMutex in GetLayerHueStep" << std::endl;
    return m_layerHueStep;
}

BLRgba32 BoardDataManager::GetColorUnlocked(ColorType type) const
{
    auto it = color_map_.find(type);
    if (it != color_map_.end())
    {
        return it->second;
    }
    switch (type)
    {
    case ColorType::kNetHighlight:
        return BLRgba32(0xFFFFFFFF); // Default to white
    case ColorType::kSilkscreen:
        return BLRgba32(0x80DDDDDD); // Default to translucent light gray
    case ColorType::kComponent:
        return BLRgba32(0x800000FF); // Default to translucent blue
    case ColorType::kPin:
        return BLRgba32(0x80999999); // Default to medium grey
    case ColorType::kBaseLayer:
        return BLRgba32(0x80007BFF); // Default to translucent blue
    case ColorType::kBoardEdges:
        return BLRgba32(0xFF00FF00); // Default to green
    default:
        return BLRgba32(0xFFFFFFFF); // Default to white
    }
}

BLRgba32 BoardDataManager::GetColor(ColorType type) const
{
    std::cout << "[BoardDataManager] Locking m_netMutex in GetColor for type " << static_cast<int>(type) << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] Acquired m_netMutex in GetColor for type " << static_cast<int>(type) << std::endl;
    return GetColorUnlocked(type);
}

void BoardDataManager::LoadColorsFromConfig(const Config &config)
{
    static const ColorType all_types[] = {
        ColorType::kNetHighlight,
        ColorType::kSilkscreen,
        ColorType::kComponent,
        ColorType::kPin,
        ColorType::kBaseLayer,
        ColorType::kBoardEdges};
    std::cout << "[BoardDataManager] Locking m_netMutex in LoadColorsFromConfig" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] Acquired m_netMutex in LoadColorsFromConfig" << std::endl;
    for (ColorType type : all_types)
    {
        std::string key = "color." + std::string(::ColorTypeToString(type));
        if (config.HasKey(key))
        {
            uint32_t rgba = static_cast<uint32_t>(config.GetInt(key, 0xFFFFFFFF));
            color_map_[type] = BLRgba32(rgba);
        }
        else
        {
            color_map_[type] = GetColorUnlocked(type); // fallback to default, no lock
        }
    }
}

void BoardDataManager::SaveColorsToConfig(Config &config) const
{
    static const ColorType all_types[] = {
        ColorType::kNetHighlight,
        ColorType::kSilkscreen,
        ColorType::kComponent,
        ColorType::kPin,
        ColorType::kBaseLayer,
        ColorType::kBoardEdges};
    std::cout << "[BoardDataManager] Locking m_netMutex in SaveColorsToConfig" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] Acquired m_netMutex in SaveColorsToConfig" << std::endl;
    for (ColorType type : all_types)
    {
        std::string key = "color." + std::string(::ColorTypeToString(type));
        auto it = color_map_.find(type);
        uint32_t rgba = (it != color_map_.end()) ? it->second.value : GetColorUnlocked(type).value;
        config.SetInt(key, static_cast<int>(rgba));
    }
}

void BoardDataManager::RegenerateLayerColors(const std::shared_ptr<Board> &board)
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in RegenerateLayerColors" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in RegenerateLayerColors" << std::endl;
    if (!board)
        return;

    int layerCount = board->GetLayerCount();
    m_layerColors.resize(layerCount);

    BLRgba32 baseColor = color_map_.count(ColorType::kBaseLayer) ? color_map_.at(ColorType::kBaseLayer) : GetColorUnlocked(ColorType::kBaseLayer);
    float hueStep = m_layerHueStep;

    for (int i = 0; i < layerCount; ++i)
    {
        BLRgba32 layerColor = ColorUtils::GenerateLayerColor(
            i,
            layerCount,
            baseColor,
            hueStep);
        m_layerColors[i] = layerColor;
        board->SetLayerColor(i, layerColor);
    }
    auto cb = m_settingsChangeCallback;
    lock.~lock_guard();
    if (cb)
    {
        cb();
    }
}

void BoardDataManager::SetSelectedNetId(int netId)
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in SetSelectedNetId" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in SetSelectedNetId" << std::endl;
    if (m_selectedNetId != netId)
    {
        m_selectedNetId = netId;
        auto cb = m_netIdChangeCallback;
        lock.~lock_guard();
        if (cb)
        {
            cb(netId);
        }
    }
}

int BoardDataManager::GetSelectedNetId() const
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    return m_selectedNetId;
}

void BoardDataManager::RegisterNetIdChangeCallback(NetIdChangeCallback callback)
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in RegisterNetIdChangeCallback" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in RegisterNetIdChangeCallback" << std::endl;
    m_netIdChangeCallback = callback;
}

void BoardDataManager::UnregisterNetIdChangeCallback()
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in UnregisterNetIdChangeCallback" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in UnregisterNetIdChangeCallback" << std::endl;
    m_netIdChangeCallback = nullptr;
}

void BoardDataManager::RegisterSettingsChangeCallback(SettingsChangeCallback callback)
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in RegisterSettingsChangeCallback" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in RegisterSettingsChangeCallback" << std::endl;
    m_settingsChangeCallback = callback;
}

void BoardDataManager::UnregisterSettingsChangeCallback()
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in UnregisterSettingsChangeCallback" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in UnregisterSettingsChangeCallback" << std::endl;
    m_settingsChangeCallback = nullptr;
}

void BoardDataManager::RegisterLayerVisibilityChangeCallback(LayerVisibilityChangeCallback callback)
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in RegisterLayerVisibilityChangeCallback" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in RegisterLayerVisibilityChangeCallback" << std::endl;
    m_layerVisibilityChangeCallback = callback;
}

void BoardDataManager::UnregisterLayerVisibilityChangeCallback()
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in UnregisterLayerVisibilityChangeCallback" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in UnregisterLayerVisibilityChangeCallback" << std::endl;
    m_layerVisibilityChangeCallback = nullptr;
}

void BoardDataManager::SetLayerVisible(int layerId, bool visible)
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in SetLayerVisible" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in SetLayerVisible" << std::endl;
    if (layerId >= 0 && layerId < static_cast<int>(m_layerVisibility.size()))
    {
        m_layerVisibility[layerId] = visible;
        auto cb = m_layerVisibilityChangeCallback;
        lock.~lock_guard();
        if (cb)
        {
            cb(layerId, visible);
        }
        if (m_settingsChangeCallback)
        {
            m_settingsChangeCallback();
        }
    }
}

void BoardDataManager::SetColor(ColorType type, BLRgba32 color)
{
    std::cout << "[BoardDataManager] TMP-DEBUG: Locking m_netMutex in SetColor" << std::endl;
    std::lock_guard<std::mutex> lock(m_netMutex);
    std::cout << "[BoardDataManager] TMP-DEBUG: Acquired m_netMutex in SetColor" << std::endl;
    color_map_[type] = color;
    auto cb = m_settingsChangeCallback;
    lock.~lock_guard();
    if (cb)
    {
        cb();
    }
}

BLRgba32 BoardDataManager::GetLayerColor(int layer_id) const
{
    // 1-16: trace layers (apply hue rotation)
    if (layer_id >= 1 && layer_id <= 16)
    {
        BLRgba32 base_color = color_map_.count(ColorType::kBaseLayer) ? color_map_.at(ColorType::kBaseLayer) : GetColorUnlocked(ColorType::kBaseLayer);
        float hue_step = m_layerHueStep;
        return ColorUtils::GenerateLayerColor(layer_id - 1, 16, base_color, hue_step);
    }
    // 17: silkscreen
    if (layer_id == 17)
    {
        return color_map_.count(ColorType::kSilkscreen) ? color_map_.at(ColorType::kSilkscreen) : GetColorUnlocked(ColorType::kSilkscreen);
    }
    // 18-27: unused, but apply hue rotation
    if (layer_id >= 18 && layer_id <= 27)
    {
        BLRgba32 base_color = color_map_.count(ColorType::kBaseLayer) ? color_map_.at(ColorType::kBaseLayer) : GetColorUnlocked(ColorType::kBaseLayer);
        float hue_step = m_layerHueStep;
        return ColorUtils::GenerateLayerColor(layer_id - 1, 16, base_color, hue_step);
    }
    // 28: board edges
    if (layer_id == 28)
    {
        return color_map_.count(ColorType::kBoardEdges) ? color_map_.at(ColorType::kBoardEdges) : GetColorUnlocked(ColorType::kBoardEdges);
    }
    // fallback: default color
    return BLRgba32(0xFF888888);
}

// TODO: Add a function to get the color of a specific layer
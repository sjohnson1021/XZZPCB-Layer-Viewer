#include "core/BoardDataManager.hpp"
#include "utils/ColorUtils.hpp"
#include <mutex>

BoardDataManager::BoardDataManager()
    : m_baseLayerColor(BLRgba32(0xFF007BFF)) // Default base color (blue)
      ,
      m_layerHueStep(30.0f) // Default hue step in degrees
      ,
      m_selectedNetId(-1), m_netHighlightColor(BLRgba32(0xFFFF0000)) // Default to bright red
{
}

BoardDataManager::~BoardDataManager()
{
    // Clean up any resources if needed
    // For now, the default cleanup of member variables is sufficient
}

std::shared_ptr<const Board> BoardDataManager::getBoard() const
{
    std::lock_guard<std::mutex> lock(m_boardMutex);
    return m_currentBoard;
}

void BoardDataManager::setBoard(std::shared_ptr<Board> board)
{
    std::lock_guard<std::mutex> lock(m_boardMutex);
    m_currentBoard = board;
}

void BoardDataManager::clearBoard()
{
    std::lock_guard<std::mutex> lock(m_boardMutex);
    m_currentBoard.reset(); // Or m_currentBoard = nullptr;
}

void BoardDataManager::SetBaseLayerColor(BLRgba32 color)
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    m_baseLayerColor = color;
    if (m_settingsChangeCallback)
    {
        m_settingsChangeCallback();
    }
}

BLRgba32 BoardDataManager::GetBaseLayerColor() const
{
    std::lock_guard<std::mutex> lock(m_boardMutex);
    return m_baseLayerColor;
}

void BoardDataManager::SetLayerHueStep(float hueStep)
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    m_layerHueStep = hueStep;
    if (m_settingsChangeCallback)
    {
        m_settingsChangeCallback();
    }
}

float BoardDataManager::GetLayerHueStep() const
{
    std::lock_guard<std::mutex> lock(m_boardMutex);
    return m_layerHueStep;
}

void BoardDataManager::RegenerateLayerColors(const std::shared_ptr<Board> &board)
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    if (!board)
        return;

    int layerCount = board->GetLayerCount();
    m_layerColors.resize(layerCount);

    // Generate and apply colors for each layer
    for (int i = 0; i < layerCount; ++i)
    {
        BLRgba32 layerColor = ColorUtils::GenerateLayerColor(
            i,
            layerCount,
            m_baseLayerColor,
            m_layerHueStep);

        // Store in our internal array
        m_layerColors[i] = layerColor;

        // Apply to the board's layer
        board->SetLayerColor(i, layerColor);
    }

    if (m_settingsChangeCallback)
    {
        m_settingsChangeCallback();
    }
}

void BoardDataManager::SetSelectedNetId(int netId)
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    if (m_selectedNetId != netId)
    {
        m_selectedNetId = netId;
        if (m_netIdChangeCallback)
        {
            m_netIdChangeCallback(netId);
        }
    }
}

int BoardDataManager::GetSelectedNetId() const
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    return m_selectedNetId;
}

void BoardDataManager::SetNetHighlightColor(BLRgba32 color)
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    m_netHighlightColor = color;
    if (m_settingsChangeCallback)
    {
        m_settingsChangeCallback();
    }
}

BLRgba32 BoardDataManager::GetNetHighlightColor() const
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    return m_netHighlightColor;
}

void BoardDataManager::RegisterNetIdChangeCallback(NetIdChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    m_netIdChangeCallback = callback;
}

void BoardDataManager::UnregisterNetIdChangeCallback()
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    m_netIdChangeCallback = nullptr;
}

void BoardDataManager::RegisterSettingsChangeCallback(SettingsChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    m_settingsChangeCallback = callback;
}

void BoardDataManager::UnregisterSettingsChangeCallback()
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    m_settingsChangeCallback = nullptr;
}

void BoardDataManager::RegisterLayerVisibilityChangeCallback(LayerVisibilityChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    m_layerVisibilityChangeCallback = callback;
}

void BoardDataManager::UnregisterLayerVisibilityChangeCallback()
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    m_layerVisibilityChangeCallback = nullptr;
}

void BoardDataManager::SetLayerVisible(int layerId, bool visible)
{
    std::lock_guard<std::mutex> lock(m_netMutex);
    if (layerId >= 0 && layerId < static_cast<int>(m_layerVisibility.size()))
    {
        m_layerVisibility[layerId] = visible;
        if (m_layerVisibilityChangeCallback)
        {
            m_layerVisibilityChangeCallback(layerId, visible);
        }
        if (m_settingsChangeCallback)
        {
            m_settingsChangeCallback();
        }
    }
}
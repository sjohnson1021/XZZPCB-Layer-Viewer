#include "core/BoardDataManager.hpp"
#include "utils/ColorUtils.hpp"
#include <mutex>

// Removed getInstance(), loadBoard(), and m_currentBoard related logic

std::shared_ptr<Board> BoardDataManager::getBoard() const {
    std::lock_guard<std::mutex> lock(m_boardMutex);
    // m_currentBoard is no longer a member. This method needs to be re-evaluated.
    // For now, returning nullptr as its original purpose is removed.
    return nullptr; 
}

void BoardDataManager::clearBoard() {
    std::lock_guard<std::mutex> lock(m_boardMutex);
    // m_currentBoard is no longer a member. This method needs to be re-evaluated.
}

void BoardDataManager::SetBaseLayerColor(BLRgba32 color) {
    std::lock_guard<std::mutex> lock(m_boardMutex);
    m_baseLayerColor = color;
}

BLRgba32 BoardDataManager::GetBaseLayerColor() const {
    std::lock_guard<std::mutex> lock(m_boardMutex);
    return m_baseLayerColor;
}

void BoardDataManager::SetLayerHueStep(float hueStep) {
    std::lock_guard<std::mutex> lock(m_boardMutex);
    m_layerHueStep = hueStep;
}

float BoardDataManager::GetLayerHueStep() const {
    std::lock_guard<std::mutex> lock(m_boardMutex);
    return m_layerHueStep;
}

void BoardDataManager::RegenerateLayerColors(std::shared_ptr<Board> board) {
    std::lock_guard<std::mutex> lock(m_boardMutex);
    if (board && !board->layers.empty()) {
        for (size_t i = 0; i < board->layers.size(); ++i) {
            board->layers[i].color = ColorUtils::GenerateLayerColor(
                static_cast<int>(i),
                static_cast<int>(board->layers.size()),
                m_baseLayerColor,
                m_layerHueStep
            );
        }
    }
} 
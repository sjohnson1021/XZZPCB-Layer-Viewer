#pragma once // Added include guard

#include <string>
#include <memory>
#include <mutex> // For thread-safe access to the board
#include <blend2d.h> // For BLRgba32
#include "../pcb/Board.hpp"
#include "../pcb/XZZPCBLoader.hpp"

// Forward declare Board if Board.hpp isn't fully needed here, but it is for shared_ptr<Board>
// class Board; 

class BoardDataManager {
public:
    // Retrieves the currently loaded board
    // Returns a shared_ptr to the board, which might be nullptr if no board is loaded
    std::shared_ptr<Board> getBoard() const;

    // Sets the currently loaded board
    void setBoard(std::shared_ptr<Board> board);

    // Clears the currently loaded board
    void clearBoard();

    // --- Layer Color Management ---
    void SetBaseLayerColor(BLRgba32 color);
    BLRgba32 GetBaseLayerColor() const;

    void SetLayerHueStep(float hueStep);
    float GetLayerHueStep() const;

    void RegenerateLayerColors(std::shared_ptr<Board> board); // Re-applies coloring to the given board

private:
    std::shared_ptr<Board> m_currentBoard; // Added member to hold the current board
    PcbLoader m_pcbLoader; // This seems out of place if BoardDataManager doesn't load boards
    mutable std::mutex m_boardMutex; // Mutex to protect access to m_currentBoard

    BLRgba32 m_baseLayerColor = BLRgba32(0xFF007BFF); // Default base color (blue)
    float m_layerHueStep = 30.0f;                   // Default hue step in degrees
};

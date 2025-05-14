#pragma once

#include "imgui.h"
#include "../../pcb/Board.hpp" // Path to our Board data model
#include <string>
#include <vector>
#include <memory>

class PcbDetailsWindow {
public:
    PcbDetailsWindow();
    void render();
    void setBoard(std::shared_ptr<Board> board);
    void SetVisible(bool visible);
    bool IsWindowVisible() const;

private:
    std::shared_ptr<Board> current_board_;
    bool is_visible_ = false;

    void displayBasicInfo(const Board* boardData);
    void displayLayers(const Board* boardData);
    void displayNets(const Board* boardData);
    void displayComponents(const Board* boardData);
    void displayPins(const std::vector<Pin>& pins);
    void displayPadShape(const PadShape& shape);
    void displayGraphicalElements(const std::vector<LineSegment>& elements);
    void displayStandaloneElements(const Board* boardData);
}; 
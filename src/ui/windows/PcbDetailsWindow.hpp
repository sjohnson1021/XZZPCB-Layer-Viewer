#pragma once

#include <memory>

#include "imgui.h"

#include "pcb/Board.hpp"  // Path to our Board data model
#include "pcb/elements/Pin.hpp"

class PcbDetailsWindow
{
public:
    PcbDetailsWindow();
    void Render();
    void SetBoard(std::shared_ptr<Board> board);
    void SetVisible(bool visible);
    [[nodiscard]] bool IsWindowVisible() const;

private:
    std::shared_ptr<Board> current_board_;
    bool is_visible_ = false;

    void DisplayBasicInfo(const Board* board_data);
    void DisplayLayers(const Board* board_data);
    void DisplayNets(const Board* board_data);
    void DisplayComponents(const Board* board_data);
    void DisplayPins(const Board* board_data, const std::vector<std::unique_ptr<Pin>>& pins);
    void DisplayPadShape(const PadShape& shape);
    void DisplayGraphicalElements(const std::vector<LineSegment>& elements);
    void DisplayStandaloneElements(const Board* board_data);
};

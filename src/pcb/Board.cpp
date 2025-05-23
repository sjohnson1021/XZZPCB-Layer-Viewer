#include "pcb/Board.hpp"
#include "pcb/BoardLoaderFactory.hpp" // Include the factory
#include <iostream> // For std::cerr or logging

// Default constructor
Board::Board() : m_isLoaded(false) {
    // Default initialization, maybe an empty board or specific default state
}

// Constructor that takes a file path
Board::Board(const std::string& filePath)
    : file_path(filePath)
    , m_isLoaded(false)
{
    if (filePath.empty()) {
        m_errorMessage = "File path is empty.";
        // m_isLoaded is already false
        return;
    }

    if (filePath == "dummy_fail.pcb") { // Keep this for testing error modal
        m_errorMessage = "This is a dummy failure to test the error modal.";
        // m_isLoaded is already false
        return;
    }

    BoardLoaderFactory factory; // Create the factory
    std::unique_ptr<Board> loaded_board_data = factory.loadBoard(filePath); // Use the factory

    if (loaded_board_data) {
        // If loading was successful, we need to move the data from loaded_board_data
        // to 'this' Board object. This is a bit tricky because loadBoard returns a new Board.
        // A more common pattern is for the loader to populate an existing Board object, or for the
        // factory to return a Board, and then we'd copy/move members.
        // For now, let's assume a simple member-wise move/copy. This needs careful review!

        // Steal the data from the loaded board.
        // This is a shallow copy for pointers/unique_ptrs and deep for values/vectors if not moved.
        *this = std::move(*loaded_board_data); // This uses Board's move assignment operator.
                                               // Ensure Board has a proper move constructor and move assignment operator.
        this->file_path = filePath; // Reset file_path as it might be overwritten by move
        this->m_isLoaded = true;
        this->m_errorMessage.clear();

        // RegenerateLayerColors might be needed if it's not handled by Board's move/copy semantics
        // or if the loaded_board_data didn't have colors generated in its context.
        // For now, assume it's handled or not immediately critical here after a move.

    } else {
        m_errorMessage = "Failed to load board from file: " + filePath + ". Check logs for details.";
        // m_isLoaded is already false
    }
}

// --- Layer Access Methods ---
std::vector<LayerInfo> Board::GetLayers() const {
    return layers;
}

int Board::GetLayerCount() const {
    return static_cast<int>(layers.size());
}

std::string Board::GetLayerName(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < layers.size()) {
        return layers[layerIndex].name;
    }
    return "Invalid Layer Index"; // Or throw an exception
}

bool Board::IsLayerVisible(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < layers.size()) {
        return layers[layerIndex].is_visible;
    }
    return false; // Or throw an exception
}

void Board::SetLayerVisible(int layerIndex, bool visible) {
    if (layerIndex >= 0 && layerIndex < layers.size()) {
        layers[layerIndex].is_visible = visible;
        // Here, you might want to trigger an event or mark the board as modified
    }
    // Else: handle error or do nothing for invalid index
}

// --- Loading Status Methods ---
bool Board::IsLoaded() const {
    return m_isLoaded;
}

std::string Board::GetErrorMessage() const {
    return m_errorMessage;
}

std::string Board::GetFilePath() const {
    return file_path;
}

const LayerInfo* Board::GetLayerById(int layerId) const {
    for (const auto& layer : layers) {
        if (layer.id == layerId) {
            return &layer;
        }
    }
    return nullptr; // Not found
}

// Calculate the bounding box based on traces on Layer 28 (Board Outline for XZZ)
BLRect Board::GetBoundingBox(bool include_invisible_layers) const {
    bool first = true;
    double min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    const int board_outline_layer_id = 28;

    const LayerInfo* outline_layer = nullptr;
    for(const auto& l : layers) {
        if (l.id == board_outline_layer_id) {
            outline_layer = &l;
            break;
        }
    }

    if (!outline_layer) {
        // Layer 28 not defined, cannot determine outline.
        return BLRect(0, 0, 0, 0); 
    }

    if (!include_invisible_layers && !outline_layer->is_visible) {
        // Layer 28 is not visible and we are not including invisible layers.
        return BLRect(0, 0, 0, 0);
    }

    auto expand_rect_from_point = [&](double x, double y) {
        if (first) {
            min_x = max_x = x;
            min_y = max_y = y;
            first = false;
        } else {
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    };

    for (const auto& trace : traces) {
        if (trace.layer == board_outline_layer_id) {
            // For traces, their width contributes to the bounding box.
            // We expand by start/end points. A more precise calculation would consider thickness.
            expand_rect_from_point(trace.x1, trace.y1);
            expand_rect_from_point(trace.x2, trace.y2);
            // Note: Trace width is not explicitly used here to expand bounds further,
            // assuming the centerlines of outline traces define the extents.
            // If trace thickness should define the outer edge, expand_rect_with_width would be needed.
        }
    }

    if (first) { // No traces found on layer 28
        return BLRect(0, 0, 0, 0);
    }
    return BLRect(min_x, min_y, max_x - min_x, max_y - min_y);
}

Board::BoardPoint2D Board::NormalizeCoordinatesAndGetCenterOffset(const BLRect& original_bounds) {
    if (original_bounds.w <= 0 || original_bounds.h <= 0) {
        return {0.0, 0.0}; // No valid bounds to normalize against
    }

    double offset_x = original_bounds.x + original_bounds.w / 2.0;
    double offset_y = original_bounds.y + original_bounds.h / 2.0;

    // Normalize Traces
    for (auto& trace : traces) {
        trace.x1 -= offset_x;
        trace.y1 -= offset_y;
        trace.x2 -= offset_x;
        trace.y2 -= offset_y;
    }

    // Normalize Vias
    for (auto& via : vias) {
        via.x -= offset_x;
        via.y -= offset_y;
    }

    // Normalize Arcs
    for (auto& arc : arcs) {
        arc.cx -= offset_x;
        arc.cy -= offset_y;
    }

    // Normalize Standalone Text Labels
    for (auto& label : standalone_text_labels) {
        label.x -= offset_x;
        label.y -= offset_y;
    }

    // Normalize Components and their sub-elements
    for (auto& comp : components) {
        comp.center_x -= offset_x;
        comp.center_y -= offset_y;

        for (auto& pin : comp.pins) {
            pin.x_coord -= offset_x;
            pin.y_coord -= offset_y;
            // Note: PadShape internal offsets (if any) are relative to pin.x_coord, pin.y_coord, so they don't need direct normalization here.
        }

        for (auto& elem : comp.graphical_elements) {
            elem.start.x -= offset_x;
            elem.start.y -= offset_y;
            elem.end.x -= offset_x;
            elem.end.y -= offset_y;
        }

        for (auto& label : comp.text_labels) {
            label.x -= offset_x;
            label.y -= offset_y;
        }
    }
    
    // Update the board's own origin_offset if you want to store this normalization offset
    this->origin_offset = {offset_x, offset_y};

    return {offset_x, offset_y};
}

// If PcbLoader is used internally by Board::ParseBoardFile (example skeleton)
// ... existing code ... 
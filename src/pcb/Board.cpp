#include "pcb/Board.hpp"
#include "pcb/PcbLoader.hpp" // Assuming PcbLoader might be used in a real implementation
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

    PcbLoader loader;
    std::unique_ptr<Board> loaded_board_data = loader.loadFromFile(filePath);

    if (loaded_board_data) {
        // Move data from loaded_board_data to this instance.
        // Note: file_path is already set for *this and should remain as the path provided to constructor.
        // loaded_board_data->file_path will also be set by the loader, usually to the same value.
        board_name = std::move(loaded_board_data->board_name);
        width = loaded_board_data->width;
        height = loaded_board_data->height;
        origin_offset = loaded_board_data->origin_offset; 

        layers = std::move(loaded_board_data->layers);
        arcs = std::move(loaded_board_data->arcs);
        vias = std::move(loaded_board_data->vias);
        traces = std::move(loaded_board_data->traces);
        standalone_text_labels = std::move(loaded_board_data->standalone_text_labels);
        components = std::move(loaded_board_data->components);
        nets = std::move(loaded_board_data->nets);

        m_isLoaded = true;
        m_errorMessage.clear();
        // std::cout << "Board loaded successfully via PcbLoader: " << filePath << std::endl; // For debugging
    } else {
        m_isLoaded = false;
        // PcbLoader::loadFromFile returns nullptr on error. 
        // A more sophisticated error reporting could be added to PcbLoader.
        m_errorMessage = "Failed to load PCB data from file: " + filePath;
        std::cerr << "PcbLoader failed to load: " << filePath << std::endl; // For debugging
    }
}

// --- Layer Access Methods ---
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

// If PcbLoader is used internally by Board::ParseBoardFile (example skeleton)
// ... existing code ... 
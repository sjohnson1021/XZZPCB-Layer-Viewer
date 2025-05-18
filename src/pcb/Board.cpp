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
    , m_isLoaded(false) // Start as not loaded
{
    // --- Dummy Implementation for now ---
    // In a real scenario, this would involve parsing the file using PcbLoader or similar.
    if (filePath.empty()) {
        m_errorMessage = "File path is empty.";
        m_isLoaded = false;
        return;
    }

    if (filePath == "dummy_fail.pcb") { // Special case for testing error modal
        m_errorMessage = "This is a dummy failure to test the error modal.";
        m_isLoaded = false;
        return;
    }

    // Simulate loading a board
    board_name = filePath.substr(filePath.find_last_of("/\\_,") + 1);

    // Add some dummy layers for UI testing
    layers.emplace_back(0, "Top Copper", LayerInfo::LayerType::Signal);
    layers.emplace_back(1, "Inner Layer 1", LayerInfo::LayerType::Signal);
    layers.emplace_back(2, "Inner Layer 2", LayerInfo::LayerType::Signal);
    layers.emplace_back(3, "Bottom Copper", LayerInfo::LayerType::Signal);
    layers.emplace_back(4, "Top Silkscreen", LayerInfo::LayerType::Silkscreen);
    layers.emplace_back(5, "Bottom Silkscreen", LayerInfo::LayerType::Silkscreen);
    layers.emplace_back(6, "Top Solder Mask", LayerInfo::LayerType::SolderMask);
    layers.emplace_back(7, "Bottom Solder Mask", LayerInfo::LayerType::SolderMask);
    layers.emplace_back(8, "Board Outline", LayerInfo::LayerType::BoardOutline);

    // Set some layers to initially not visible for testing
    if (layers.size() > 1) layers[1].is_visible = false;
    if (layers.size() > 4) layers[4].is_visible = false;

    width = 100.0; // dummy data
    height = 80.0; // dummy data

    m_isLoaded = true;
    m_errorMessage.clear();
    std::cout << "Board created (dummy load): " << filePath << std::endl;

    // Example of how PcbLoader might be used (commented out for dummy implementation)
    /*
    PcbLoader loader;
    bool success = loader.loadFromFile(filePath, *this); // PcbLoader would populate this Board object
    if (success) {
        m_isLoaded = true;
        m_errorMessage.clear();
    } else {
        m_isLoaded = false;
        m_errorMessage = loader.getLastError(); // Assuming PcbLoader has such a method
        std::cerr << "Failed to load board from " << filePath << ": " << m_errorMessage << std::endl;
    }
    */
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
#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include "Board.hpp"  // Our main board data model
#include "IBoardLoader.hpp"

#include "pcb/elements/Arc.hpp"
#include "pcb/elements/Pin.hpp"
#include "pcb/elements/TextLabel.hpp"
#include "pcb/elements/Trace.hpp"
#include "pcb/elements/Via.hpp"

// Forward declaration for internal helper, if needed later
// struct PcbFileHeaderInfo;

class PcbLoader : public IBoardLoader
{
public:
    PcbLoader() = default;

    // Main public method to load a PCB file
    std::unique_ptr<Board> LoadFromFile(const std::string& file_path) override;

private:
    // --- File Processing Stages ---
    bool ReadFileData(const std::string& file_path, std::vector<char>& file_data);  // Reads entire file
    bool VerifyFormat(const std::vector<char>& file_data);
    bool DecryptFileDataIfNeeded(std::vector<char>& file_data);  // Handles XOR and DES for components

    // --- Core Parsing Functions ---
    // These will populate the passed-in Board object
    bool
    ParseHeader(const std::vector<char>& file_data, Board& board, uint32_t& out_main_data_offset, uint32_t& out_net_data_offset, uint32_t& out_image_data_offset, uint32_t& out_main_data_blocks_size);

    bool ParseMainDataBlocks(const std::vector<char>& file_data, Board& board, uint32_t main_data_offset, uint32_t main_data_blocks_size);

    bool ParseNetBlock(const std::vector<char>& file_data, Board& board, uint32_t net_data_offset);

    bool ParsePostV6Block(const std::vector<char>& file_data, Board& board,
                          std::vector<char>::const_iterator v6_pos);  // For diode readings etc.

    // --- Element Specific Parsers (called by parseMainDataBlocks) ---
    // These will convert raw data to our new element structs/classes and add to the Board
    static void ParseArc(const char* data, Board& board);
    static void ParseVia(const char* data, uint32_t block_size, Board& board);
    static void ParseTrace(const char* data, Board& board);
    static void ParseTextLabel(const char* data, Board& board, bool is_standalone);
    void ParseComponent(const char* data, uint32_t component_block_size, Board& board);

    // --- Layer Definition Helper ---
    static void DefineStandardLayers(Board& board);

    // --- Decryption Helpers (specific to component data in this format) ---
    void DecryptComponentBlock(std::vector<char>& component_data);  // Uses the DES function

    // --- String/Character Encoding Helpers ---
    static std::string ReadCB2312String(const char* data, size_t length);
    // char readUtf8Char(char c) const; // May not be needed if CB2312 conversion handles it

    // --- Low-level Data Reading ---
    template <typename T>
    static T ReadLE(const char* data)
    {
        T value;
        memcpy(&value, data, sizeof(T));
        return value;
    }

    // Internal state if any, e.g., for diode readings across parsing stages
    int diode_readings_type_ = 0;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> diode_readings_ {};
    // The key for DES decryption of component blocks is derived within decryptComponentBlock
};

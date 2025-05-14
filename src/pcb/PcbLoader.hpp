#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include "Board.hpp"       // Our main board data model
#include "../utils/des.h"  // For the DES decryption function

// Forward declaration for internal helper, if needed later
// struct PcbFileHeaderInfo;

class PcbLoader {
public:
    PcbLoader() = default;

    // Main public method to load a PCB file
    std::unique_ptr<Board> loadFromFile(const std::string& filePath);

private:
    // --- File Processing Stages ---
    bool readFileData(const std::string& filePath, std::vector<char>& fileData); // Reads entire file
    bool verifyFormat(const std::vector<char>& fileData);
    bool decryptFileDataIfNeeded(std::vector<char>& fileData); // Handles XOR and DES for components
    
    // --- Core Parsing Functions ---
    // These will populate the passed-in Board object
    bool parseHeader(const std::vector<char>& fileData, Board& board, 
                     uint32_t& outMainDataOffset, uint32_t& outNetDataOffset, 
                     uint32_t& outImageDataOffset, uint32_t& outMainDataBlocksSize);

    bool parseMainDataBlocks(const std::vector<char>& fileData, Board& board, 
                             uint32_t mainDataOffset, uint32_t mainDataBlocksSize);

    bool parseNetBlock(const std::vector<char>& fileData, Board& board, uint32_t netDataOffset);
    
    bool parsePostV6Block(const std::vector<char>& fileData, Board& board, 
                          std::vector<char>::const_iterator v6_pos); // For diode readings etc.

    // --- Element Specific Parsers (called by parseMainDataBlocks) ---
    // These will convert raw data to our new element structs/classes and add to the Board
    void parseArc(const char* data, Board& board);
    void parseVia(const char* data, Board& board);
    void parseTrace(const char* data, Board& board);
    void parseTextLabel(const char* data, Board& board, bool isStandalone);
    void parseComponent(const char* data, uint32_t componentBlockSize, Board& board);

    // --- Decryption Helpers (specific to component data in this format) ---
    void decryptComponentBlock(std::vector<char>& componentData); // Uses the DES function

    // --- String/Character Encoding Helpers ---
    std::string readCB2312String(const char* data, size_t length);
    // char readUtf8Char(char c) const; // May not be needed if CB2312 conversion handles it

    // --- Low-level Data Reading ---
    template<typename T>
    T readLE(const char* data) {
        T value;
        memcpy(&value, data, sizeof(T));
        return value;
    }

    // Internal state if any, e.g., for diode readings across parsing stages
    int diodeReadingsType_ = 0;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> diodeReadings_;
    // The key for DES decryption of component blocks is derived within decryptComponentBlock
}; 
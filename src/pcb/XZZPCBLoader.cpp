#include "XZZPCBLoader.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>               // For std::search
#include <cstring>                 // For std::memcpy
#include <iomanip>                 // For std::setw, std::setfill, std::hex
#include <sstream>                 // For std::ostringstream
#include <limits>                  // For std::numeric_limits
#include "../utils/ColorUtils.hpp" // Added for layer color generation
#include <iostream>                // Added for logging
#include <variant>                 // Required for std::visit or index() on PadShape
#include "processing/OrientationProcessor.hpp"

// Define this to enable verbose logging for PcbLoader
// #define ENABLE_PCB_LOADER_LOGGING

// Static data for DES key generation, similar to old XZZXZZPCBLoader.cpp
static const unsigned char hexconv[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const std::vector<uint16_t> des_key_byte_list = {0xE0, 0xCF, 0x2E, 0x9F, 0x3C, 0x33, 0x3C, 0x33};
static const int PIN_LAYER = 29;
static const int VIA_LAYER = 30;
// This is the scale factor for the XZZ files.
static const int xyscale = 100000;
// Implementation of DefineStandardLayers
void PcbLoader::DefineStandardLayers(Board &board)
{
    // Clear any existing layers first, in case this is a reload or reused Board object
    board.layers.clear();

    // Define layer for pins
    board.addLayer(LayerInfo(PIN_LAYER, "Pins", LayerInfo::LayerType::Signal));
    board.addLayer(LayerInfo(VIA_LAYER, "Vias", LayerInfo::LayerType::Signal));
    // Define Trace Layers (1-16)
    for (int i = 1; i <= 16; ++i)
    {
        std::string name = "Trace Layer " + std::to_string(i);
        // Layer 16 is often special (e.g., last layer in a sequence)
        // For now, all are Signal type. Specific XZZ usage might refine this.
        board.addLayer(LayerInfo(i, name, LayerInfo::LayerType::Signal));
    }

    // Silkscreen (17)
    board.addLayer(LayerInfo(17, "Silkscreen", LayerInfo::LayerType::Silkscreen));

    // Unknown Layers (18-27) - treat as generic 'Other' or 'Comment' type for now
    for (int i = 18; i <= 27; ++i)
    {
        std::string name = "Unknown Layer " + std::to_string(i);
        // These might be used for internal notes, mechanical details, or additional silkscreen
        // Defaulting to 'Other' type. Users can inspect content.
        board.addLayer(LayerInfo(i, name, LayerInfo::LayerType::Other));
    }

    // Board Edges (28)
    board.addLayer(LayerInfo(28, "Board Edges", LayerInfo::LayerType::BoardOutline));

    // After adding all layers, if your Board class has a function to assign unique colors:
    // board.RegenerateLayerColors(); // Or similar, if it exists in Board.hpp
}

std::unique_ptr<Board> PcbLoader::loadFromFile(const std::string &filePath)
{
    std::vector<char> fileData;
    if (!readFileData(filePath, fileData))
    {
        // Consider logging an error here
        return nullptr;
    }

    if (!verifyFormat(fileData))
    {
        // Consider logging an error here
        return nullptr;
    }

    if (!decryptFileDataIfNeeded(fileData))
    {
        // Consider logging an error here
        return nullptr;
    }

    auto board = std::make_unique<Board>();
    board->file_path = filePath;
    // board->board_name = extract from file path or header?
    DefineStandardLayers(*board);

    uint32_t mainDataOffset = 0;
    uint32_t netDataOffset = 0;
    uint32_t imageDataOffset = 0; // Currently unused by parser logic beyond header
    uint32_t mainDataBlocksSize = 0;

    if (!parseHeader(fileData, *board, mainDataOffset, netDataOffset, imageDataOffset, mainDataBlocksSize))
    {
        return nullptr; // Error parsing header
    }

    if (!parseMainDataBlocks(fileData, *board, mainDataOffset, mainDataBlocksSize))
    {
        return nullptr; // Error parsing main data blocks
    }

    if (!parseNetBlock(fileData, *board, netDataOffset))
    {
        return nullptr; // Error parsing net block
    }

    // Handle post-v6 data if present (diode readings)
    static const std::vector<uint8_t> v6_marker = {0x76, 0x36, 0x76, 0x36, 0x35, 0x35, 0x35, 0x76, 0x36, 0x76, 0x36};
    auto v6_iter = std::search(fileData.cbegin(), fileData.cend(), v6_marker.cbegin(), v6_marker.cend());
    if (v6_iter != fileData.cend())
    {
        if (!parsePostV6Block(fileData, *board, v6_iter))
        {
            // Optional: log error but continue, as this might be auxiliary data
        }
    }

    // Board is populated. Now calculate its bounds and normalize coordinates.
    BLRect original_extents = board->GetBoundingBox(true); // Use layer 28 traces, include even if layer 28 is initially hidden

    if (original_extents.w > 0 || original_extents.h > 0)
    { // Allow for 1D lines to still have a center
        board->origin_offset = board->NormalizeCoordinatesAndGetCenterOffset(original_extents);
        // Store the calculated dimensions on the board object itself
        board->width = original_extents.w;
        board->height = original_extents.h;
    }
    else
    {
        // If no valid outline on layer 28 (or board is empty/point-like),
        // dimensions remain 0 or default. Normalization is skipped.
        // board->width and board->height will retain their initial values (likely 0.0).
        // Optionally, log a warning if an outline was expected for an XZZ file.
        // std::cerr << "Warning: Could not determine board bounds from layer 28 outline." << std::endl;
    }

    // The Board::Board(filePath) constructor is responsible for setting m_isLoaded.
    // By reaching this point in the loader, we assume the loading process itself was successful
    // in terms of reading and parsing data into the board structure.

    // Process pin orientations
    PCBProcessing::OrientationProcessor::processBoard(*board);

    return board;
}

bool PcbLoader::readFileData(const std::string &filePath, std::vector<char> &fileData)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    fileData.resize(static_cast<size_t>(fileSize));
    if (!file.read(fileData.data(), fileSize))
    {
        fileData.clear();
        return false;
    }
    return true;
}

bool PcbLoader::verifyFormat(const std::vector<char> &fileData)
{
    if (fileData.size() < 6)
        return false;
    const char *signature = "XZZPCB";
    if (std::memcmp(fileData.data(), signature, 6) == 0)
        return true;

    // Check for XORed encrypted format
    if (fileData.size() >= 0x11)
    { // Need at least 0x10 + 1 byte
        uint8_t xor_key = static_cast<uint8_t>(fileData[0x10]);
        if (xor_key != 0x00)
        { // If 0x00, it's not XORed this way or already decrypted
            bool match = true;
            for (int i = 0; i < 6; ++i)
            {
                if ((static_cast<uint8_t>(fileData[i]) ^ xor_key) != static_cast<uint8_t>(signature[i]))
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return true;
        }
    }
    return false;
}

// Handles the initial XOR decryption of the entire file if needed.
// The DES decryption for component blocks is handled separately in parseComponent.
bool PcbLoader::decryptFileDataIfNeeded(std::vector<char> &fileData)
{
    if (fileData.size() < 0x11 || static_cast<uint8_t>(fileData[0x10]) == 0x00)
    {
        return true; // Not encrypted this way or too small
    }

    uint8_t xor_key = static_cast<uint8_t>(fileData[0x10]);

    // The v6 marker search helps to avoid XORing the post-v6 block if it exists
    // This matches the logic in the original XZZPCBLoader
    static const std::vector<uint8_t> v6_marker = {0x76, 0x36, 0x76, 0x36, 0x35, 0x35, 0x35, 0x76, 0x36, 0x76, 0x36};
    auto v6_iter = std::search(fileData.begin(), fileData.end(), v6_marker.begin(), v6_marker.end());

    size_t end_offset = (v6_iter != fileData.end()) ? std::distance(fileData.begin(), v6_iter) : fileData.size();

    for (size_t i = 0; i < end_offset; ++i)
    {
        fileData[i] ^= xor_key;
    }
    // Crucially, after XORing, the byte at 0x10 should become 0x00 if it was correctly decrypted.
    // This is implicitly handled as fileData[0x10] ^ xor_key will be 0 if it was xor_key.
    return true;
}

// This method is specifically for DES-decrypting component data blocks.
void PcbLoader::decryptComponentBlock(std::vector<char> &blockData)
{
    std::ostringstream key_hex_stream;
    key_hex_stream.str().reserve(16); // DES key is 8 bytes = 16 hex chars

    for (size_t i = 0; i < des_key_byte_list.size(); i += 2)
    {
        uint16_t value = (des_key_byte_list[i] << 8) | des_key_byte_list[i + 1];
        value ^= 0x3C33; // Specific XOR for this key derivation
        key_hex_stream << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
    }
    std::string key_hex_str = key_hex_stream.str();

    uint64_t des_key = 0;
    for (int i = 0; i < 8; ++i)
    {
        unsigned char byte_str[3] = {static_cast<unsigned char>(key_hex_str[i * 2]), static_cast<unsigned char>(key_hex_str[i * 2 + 1]), '\0'};
        uint64_t byte_val = hexconv[byte_str[0]] * 16 + hexconv[byte_str[1]];
        des_key |= (byte_val << ((7 - i) * 8));
    }

    std::vector<char> decrypted_data;
    decrypted_data.reserve(blockData.size());

    const char *p = blockData.data();
    const char *ep = p + blockData.size();

    while (p < ep)
    {
        if (std::distance(p, ep) < 8)
            break; // Not enough data for a full DES block

        uint64_t encrypted_block64 = 0;
        // Read block as big-endian for DES function
        for (int i = 0; i < 8; ++i)
        {
            encrypted_block64 = (encrypted_block64 << 8) | static_cast<uint8_t>(p[i]);
        }

        uint64_t decrypted_block64 = des(encrypted_block64, des_key, 'd');

        // Write decrypted block back (DES output is big-endian)
        for (int i = 7; i >= 0; --i)
        {
            decrypted_data.push_back(static_cast<char>((decrypted_block64 >> (i * 8)) & 0xFF));
        }
        p += 8;
    }
    blockData = decrypted_data; // Replace original with decrypted version
}

std::string PcbLoader::readCB2312String(const char *data, size_t length)
{
    std::string result;
    result.reserve(length); // Approximate reservation
    bool last_was_high_byte = false;

    for (size_t i = 0; i < length; ++i)
    {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < 0x80)
        { // ASCII
            result += static_cast<char>(c);
            last_was_high_byte = false;
        }
        else
        { // Potentially part of a multi-byte character (e.g., GB2312)
            // For now, treat non-ASCII as '?'
            // A proper GB2312 to UTF-8 conversion would be more robust.
            if (!last_was_high_byte)
            { // First byte of a potential 2-byte char
                result += '?';
            }
            last_was_high_byte = !last_was_high_byte;
        }
    }
    return result;
}

bool PcbLoader::parseHeader(const std::vector<char> &fileData, Board &board,
                            uint32_t &outMainDataOffset, uint32_t &outNetDataOffset,
                            uint32_t &outImageDataOffset, uint32_t &outMainDataBlocksSize)
{
    if (fileData.size() < 0x44)
    {                 // Minimum size for the header fields we need
        return false; // Error: File too small for header
    }

    try
    {
        // According to XZZPCB Decrypted.hexpat and old XZZXZZPCBLoader.cpp:
        // - image_block_start is at 0x24, relative to 0x20
        // - net_block_start is at 0x28, relative to 0x20
        // - main_data_blocks_size is at 0x40 (absolute address in fileData)

        // uint32_t header_addresses_size = readLE<uint32_t>(&fileData[0x20]); // This was in old loader, seems unused for critical offsets by old loader.

        // Aligning with XZZXZZPCBLoader.cpp: offsets are added to 0x20 unconditionally
        // if the file format implies these fields are always relative offsets stored at 0x24/0x28.
        // If an offset of 0 at 0x24 truly means "no image block and 0 is not relative to 0x20",
        // then the previous conditional logic was safer. Assuming old loader was correct:
        outImageDataOffset = readLE<uint32_t>(&fileData[0x24]) + 0x20;
        outNetDataOffset = readLE<uint32_t>(&fileData[0x28]) + 0x20;

        // The main data block content starts immediately after its size field.
        // The size field itself is at absolute offset 0x40.
        outMainDataOffset = 0x40;
        outMainDataBlocksSize = readLE<uint32_t>(&fileData[outMainDataOffset]);

        // Basic validation of offsets (crude check, can be improved)
        if (outMainDataOffset + 4 + outMainDataBlocksSize > fileData.size() ||
            (outNetDataOffset > 0 && outNetDataOffset > fileData.size()) ||
            (outImageDataOffset > 0 && outImageDataOffset > fileData.size()))
        {
            // Potentially corrupted offsets or file too small for declared sizes
            // However, some files might have 0 for net/image offsets if blocks are absent.
            // The main block size is most critical here.
            if (outMainDataOffset + 4 + outMainDataBlocksSize > fileData.size() && outMainDataBlocksSize > 0)
            {
                return false; // Main data block seems to exceed file bounds
            }
        }

        // Extract board name from file path if possible (simple version)
        size_t last_slash = board.file_path.find_last_of("/\\");
        if (last_slash != std::string::npos)
        {
            board.board_name = board.file_path.substr(last_slash + 1);
        }
        else
        {
            board.board_name = board.file_path;
        }
        // Remove extension
        size_t dot_pos = board.board_name.rfind('.');
        if (dot_pos != std::string::npos)
        {
            board.board_name = board.board_name.substr(0, dot_pos);
        }

        return true;
    }
    catch (const std::exception &e)
    {
        // Log e.what()
        return false;
    }
}

bool PcbLoader::parseMainDataBlocks(const std::vector<char> &fileData, Board &board,
                                    uint32_t mainDataOffset, uint32_t mainDataBlocksSize)
{
    if (mainDataBlocksSize == 0)
        return true; // No main data to parse

    // The actual data starts after the mainDataBlocksSize field (4 bytes)
    uint32_t currentOffset = mainDataOffset + 4;
    uint32_t endOffsetOfDataRegion = currentOffset + mainDataBlocksSize;

    try
    {
        while (currentOffset < endOffsetOfDataRegion)
        {
            // Ensure there's enough data for at least a block type and size, plus potential padding.
            if (currentOffset + 5 > fileData.size() || currentOffset + 5 > endOffsetOfDataRegion)
            {
                // Not enough data for a full block header or past declared region end.
                // This could be trailing padding or an issue.
                // For now, we assume valid files won't have partial blocks here.
                break;
            }

            // Handle potential 4-byte null padding between blocks
            if (readLE<uint32_t>(&fileData[currentOffset]) == 0)
            {
                currentOffset += 4;
                continue;
            }

            uint8_t blockType = static_cast<uint8_t>(fileData[currentOffset]);
            currentOffset += 1;

            uint32_t blockSize = readLE<uint32_t>(&fileData[currentOffset]);
            currentOffset += 4;

            // Boundary check for the block content itself
            if (currentOffset + blockSize > fileData.size() || currentOffset + blockSize > endOffsetOfDataRegion)
            {
                // Block claims to extend beyond file data or its allocated region
                return false; // Critical error
            }

            const char *blockDataStart = &fileData[currentOffset];

            switch (blockType)
            {
            case 0x01: // Arc
                parseArc(blockDataStart, board);
                break;
            case 0x02: // Via
                parseVia(blockDataStart, blockSize, board);
                break;
            case 0x03: // Unknown type_03 from hexpat, skip for now
                // std::cout << "Skipping unknown block type 0x03 of size " << blockSize << std::endl;
                break;
            case 0x05: // Trace (Line Segment in hexpat for traces)
                parseTrace(blockDataStart, board);
                break;
            case 0x06: // Text Label (standalone)
                parseTextLabel(blockDataStart, board, true /* isStandalone */);
                break;
            case 0x07: // Component
                parseComponent(blockDataStart, blockSize, board);
                break;
            case 0x09: // Test Pad / Drill Hole (type_09 in hexpat) - Treat as Via for now?
                // Or create a new element type like 'DrillHole' or 'TestPad'
                // For now, let's define a placeholder or skip.
                // std::cout << "Skipping block type 0x09 (TestPad/DrillHole) of size " << blockSize << std::endl;
                parseVia(blockDataStart, blockSize, board); // TEMPORARY: treat as via. TODO: Revisit this.
                break;
            default:
                // Unknown or unhandled block type
                // std::cout << "Skipping unknown block type 0x" << std::hex << (int)blockType << std::dec << " of size " << blockSize << std::endl;
                break;
            }
            currentOffset += blockSize;
        }
        return true;
    }
    catch (const std::runtime_error &e)
    { // Catch potential errors from readLE
        // Log e.what()
        return false;
    }
}

void PcbLoader::parseArc(const char *data, Board &board)
{
    // Structure from XZZPCBLoader.h & .hexpat type_01:
    // u32 layer;
    // u32 x1; (center x)
    // u32 y1; (center y)
    // s32 r; (radius)
    // s32 angle_start;
    // s32 angle_end;
    // s32 scale; (thickness/width)
    // s32 net_index;
    // s32 unknown_arc; (offset 28, size 4, total size 32 before unknown_arc, 36 with it)

    int layer_id = static_cast<int>(readLE<uint32_t>(data));
    double cx = static_cast<double>(readLE<uint32_t>(data + 4)) / xyscale;
    double cy = static_cast<double>(readLE<uint32_t>(data + 8)) / xyscale;
    double radius = static_cast<double>(readLE<int32_t>(data + 12)) / xyscale;
    // Correct angle scaling: XZZ format likely stores angles scaled by 10000 (degrees * 10000)
    double start_angle = static_cast<double>(readLE<int32_t>(data + 16)) / 10000.0;
    double end_angle = static_cast<double>(readLE<int32_t>(data + 20)) / 10000.0;
    double thickness = static_cast<double>(readLE<int32_t>(data + 24)) / xyscale; // Assuming 'scale' is thickness
    int net_id = static_cast<int>(readLE<int32_t>(data + 28));
    // int32_t unknown_arc_val = readLE<int32_t>(data + 32); // If needed

    Arc arc(layer_id, cx, cy, radius, start_angle, end_angle);
    arc.thickness = thickness;
    arc.net_id = net_id;
    board.addArc(arc);
}

void PcbLoader::parseVia(const char *data, uint32_t blockSize, Board &board)
{
    // Structure from XZZPCBLoader.h & .hexpat type_02:
    // s32 x;
    // s32 y;
    // s32 layer_a_radius; (pad radius on start layer)
    // s32 layer_b_radius; (pad radius on end layer)
    // u32 layer_a_index;  (start layer)
    // u32 layer_b_index;  (end layer)
    // u32 net_index;
    // u32 via_text_length; (Typically 0 or 1, if 1, an extra u8 for text)
    // char via_text[via_text_length];
    double x = static_cast<double>(readLE<int32_t>(data)) / xyscale;
    double y = static_cast<double>(readLE<int32_t>(data + 4)) / xyscale;
    double radius_a = static_cast<double>(readLE<int32_t>(data + 8)) / xyscale;  // Pad radius on layer_a
    double radius_b = static_cast<double>(readLE<int32_t>(data + 12)) / xyscale; // Pad radius on layer_b
    int layer_a = static_cast<int>(readLE<uint32_t>(data + 16));
    int layer_b = static_cast<int>(readLE<uint32_t>(data + 20));
    int net_id = static_cast<int>(readLE<uint32_t>(data + 24));
    uint32_t text_len = readLE<uint32_t>(data + 28);

    // For our Via class, we might simplify to one pad_radius if they are usually the same,
    // or keep separate if they can differ. The current Via.hpp has pad_radius_from, pad_radius_to.
    // It also has drill_diameter, which isn't directly in this old struct.
    // We'll use the larger of radius_a/radius_b for pad radii and perhaps infer drill later or set a default.

    Via via(x, y, layer_a, layer_b, radius_a, radius_b);
    via.net_id = net_id;
    // via.drill_diameter = default or calculated? For now, Via.hpp has a default.

    if (text_len > 0)
    {
        // Validate text_len
        // Check 1: Does text_len cause read beyond current block?
        // The text itself starts at offset 32 within the block data.
        if (32 + text_len > blockSize)
        {
            // Error: declared text length exceeds block boundary.
            // Log this or handle it. For now, set text to empty and clear text_len.
            // Consider logging: fprintf(stderr, "Warning: Via text_len (%u) at data offset %p would exceed block size (%u). Clamping text_len to 0.\n", text_len, (void*)data, blockSize);
            via.optional_text = "";
            text_len = 0;
        }

        // Check 2: Is text_len itself an absurdly large number, even if it might fit the block?
        // (e.g. if blocksize itself was corrupted to be huge).
        // The comment "Typically 0 or 1" suggests very small lengths.
        // Let's define a sane maximum, e.g., 1024 bytes for a via text.
        const uint32_t MAX_SANE_VIA_TEXT_LEN = 1024;
        if (text_len > MAX_SANE_VIA_TEXT_LEN && text_len != 0)
        { // text_len might have been set to 0 by previous check
            // Log this or handle it.
            // Consider logging: fprintf(stderr, "Warning: Via text_len (%u) is unusually large (max sane: %u). Clamping text_len to 0.\n", text_len, MAX_SANE_VIA_TEXT_LEN);
            via.optional_text = "";
            text_len = 0;
        }

        if (text_len > 0)
        { // Re-check after potential clamping
            // Ensure not to read past block boundary, though blockSize check in parseMainDataBlocks helps.
            // The via_text_length in hexpat for type_02 says it's typically 0 or 1, meaning a single char.
            // If it's truly a length, then data + 32 is the start of text.
            // The old XZZPCBLoader has: via.text = std::string(&fileData[currentOffset + 32], textLen);
            // So, text_len is indeed the length of the string starting at offset 32 from blockDataStart.
            via.optional_text = readCB2312String(data + 32, text_len);
        }
    }
    board.addVia(via);
}

void PcbLoader::parseTrace(const char *data, Board &board)
{
    // Structure from XZZPCBLoader.h & .hexpat type_05 (Line Segment):
    // u32 layer;
    // s32 x1;
    // s32 y1;
    // s32 x2;
    // s32 y2;
    // s32 scale; (width)
    // u32 trace_net_index;
    // Total size 28 bytes.

    int layer_id = static_cast<int>(readLE<uint32_t>(data));
    double x1 = static_cast<double>(readLE<int32_t>(data + 4)) / xyscale;
    double y1 = static_cast<double>(readLE<int32_t>(data + 8)) / xyscale;
    double x2 = static_cast<double>(readLE<int32_t>(data + 12)) / xyscale;
    double y2 = static_cast<double>(readLE<int32_t>(data + 16)) / xyscale;
    double width = static_cast<double>(readLE<int32_t>(data + 20)) / xyscale; // Assuming 'scale' is width
    int net_id = static_cast<int>(readLE<uint32_t>(data + 24));

    Trace trace(layer_id, x1, y1, x2, y2, width);
    trace.net_id = net_id;
    board.addTrace(trace);
}

void PcbLoader::parseTextLabel(const char *data, Board &board, bool isStandalone)
{
    // Structure from .hexpat type_06 (standalone Text):
    // u32 unknown_1; (layer in old XZZPCBLoader, matches hexpat part_sub_type_06 layer)
    // u32 pos_x;
    // u32 pos_y;
    // u32 text_size; (font size)
    // u32 divider;   (scale in old XZZPCBLoader & hexpat part_sub_type_06 font_scale)
    // u32 empty; (padding in hexpat part_sub_type_06)
    // u16 one; (padding in hexpat part_sub_type_06)
    // u32 text_length;
    // char text[text_length];
    // Offsets from XZZXZZPCBLoader.cpp for standalone text (type 0x06):
    // label.layer = readLE<uint32_t>(&fileData[currentOffset]); -> data + 0
    // label.x = readLE<int32_t>(&fileData[currentOffset + 4]); -> data + 4
    // label.y = readLE<int32_t>(&fileData[currentOffset + 8]); -> data + 8
    // label.fontSize = readLE<uint32_t>(&fileData[currentOffset + 12]); -> data + 12 (text_size)
    // textLen = readLE<uint32_t>(&fileData[currentOffset + 24]); -> data + 24 (text_length)
    // text_start = data + 28
    // The `scale` in old TextLabel struct came from part_sub_type_06 `font_scale` or type_06 `divider`.
    // Visibility and ps06flag are only in part_sub_type_06.

    int layer_id = static_cast<int>(readLE<uint32_t>(data));                // unknown_1 in hexpat, but layer in old loader
    double x = static_cast<double>(readLE<uint32_t>(data + 4) / xyscale);   // pos_x
    double y = static_cast<double>(readLE<uint32_t>(data + 8) / xyscale);   // pos_y
    double font_size = static_cast<double>(readLE<uint32_t>(data + 12));    // text_size
    double scale_factor = static_cast<double>(readLE<uint32_t>(data + 16)); // divider in hexpat, was text_scale in TextLabel.hpp
    // data + 20 is 'empty' (4 bytes), data + 24 is 'one' (2 bytes) then text_length (4 bytes from old loader) - this is slightly different from hexpat
    // XZZPCBLoader textLen was at +24. Text started at +28.
    // Let's trust XZZPCBLoader reading logic: fontSize @+12, textLen @+24, text @+28
    // This implies there are 12 bytes between fontSize and textLen (16, 20, 22 - this doesn't fit types) or between fontSize and text.
    // Old TextLabel struct: layer, x, y, fontSize, scale, visibility, ps06flag, text.
    // Hexpat standalone type_06: unknown_1 (layer), pos_x, pos_y, text_size (fontSize), divider (scale?), empty, one, text_length, text.
    // For standalone, we use fields based on old loader interpretation of type_06 block.

    uint32_t text_len = readLE<uint32_t>(data + 24);
    std::string text_content = readCB2312String(data + 28, text_len);

    TextLabel label(text_content, x, y, layer_id, font_size);
    label.scale = scale_factor; // From 'divider' field, which likely was a scale.
    // Standalone text labels from type 0x06 don't have visibility/ps06_flag in the same way as component text.
    // These are set to defaults in TextLabel.hpp.

    if (isStandalone)
    {
        board.addStandaloneTextLabel(label);
    }
    else
    {
        // This case should ideally not be hit if parseComponent calls a different variant or populates directly.
        // However, if called from component parsing, the component itself should add it.
        // For now, we'll assume if !isStandalone, it means it was parsed inside a component
        // and the component logic will handle adding it to itself. This function just creates it.
        // To actually support this, the function should return TextLabel or take a Component&.
        // Let's re-evaluate when doing parseComponent. For now, it adds to board if standalone.
    }
}

void PcbLoader::parseComponent(const char *rawComponentData, uint32_t componentBlockSize, Board &board)
{
    // Component data (type 0x07) is typically DES-encrypted.
    // First, make a mutable copy of the component's block data to decrypt it.
#ifdef ENABLE_PCB_LOADER_LOGGING
    std::cout << "[PcbLoader LOG] ----- Begin Component Parse -----" << std::endl;
    std::cout << "[PcbLoader LOG] Original componentBlockSize: " << componentBlockSize << std::endl;
#endif

    std::vector<char> componentData(rawComponentData, rawComponentData + componentBlockSize);
    decryptComponentBlock(componentData); // Decrypts in-place
#ifdef ENABLE_PCB_LOADER_LOGGING
    std::cout << "[PcbLoader LOG] Decrypted componentData size: " << componentData.size() << std::endl;
#endif

    uint32_t localOffset = 0;

    // Read outer component structure based on .hexpat type_07
    // uint32_t t07_block_size = componentBlockSize; (already known)
    if (componentData.size() < localOffset + 4)
    {
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cerr << "[PcbLoader LOG] Error: Not enough data for part_overall_size. componentData size: " << componentData.size() << ", localOffset: " << localOffset << std::endl;
#endif
        return;
    }
    uint32_t part_overall_size = readLE<uint32_t>(&componentData[localOffset]); // Size of the actual part data within this block
#ifdef ENABLE_PCB_LOADER_LOGGING
    std::cout << "[PcbLoader LOG] part_overall_size: " << part_overall_size << " at offset " << localOffset << std::endl;
#endif
    localOffset += 4;

    if (componentData.size() < localOffset + 4)
    {
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cerr << "[PcbLoader LOG] Error: Not enough data for padding after part_overall_size. componentData size: " << componentData.size() << ", localOffset: " << localOffset << std::endl;
#endif
        return;
    }
    localOffset += 4; // Skip padding (4 bytes)
#ifdef ENABLE_PCB_LOADER_LOGGING
    std::cout << "[PcbLoader LOG] Skipped padding. localOffset: " << localOffset << std::endl;
#endif

    if (componentData.size() < localOffset + 4)
    { /*part_x*/
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cerr << "[PcbLoader LOG] Error: Not enough for part_x" << std::endl;
#endif
        return;
    }
    double part_x = static_cast<double>(readLE<uint32_t>(&componentData[localOffset])) / xyscale;
    localOffset += 4;
    if (componentData.size() < localOffset + 4)
    { /*part_y*/
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cerr << "[PcbLoader LOG] Error: Not enough for part_y" << std::endl;
#endif
        return;
    }
    double part_y = static_cast<double>(readLE<uint32_t>(&componentData[localOffset])) / xyscale;
    localOffset += 4;
    if (componentData.size() < localOffset + 4)
    { /*scale/padding*/
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cerr << "[PcbLoader LOG] Error: Not enough for scale/padding" << std::endl;
#endif
        return;
    }
    localOffset += 4; // Skip scale/padding (4 bytes)
    if (componentData.size() < localOffset + 2)
    { /*flags*/
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cerr << "[PcbLoader LOG] Error: Not enough for flags" << std::endl;
#endif
        return;
    }
    localOffset += 2; // Skip flags
#ifdef ENABLE_PCB_LOADER_LOGGING
    std::cout << "[PcbLoader LOG] Parsed part_x: " << part_x << ", part_y: " << part_y << ". localOffset: " << localOffset << std::endl;
#endif

    uint32_t pad_size_len = 0;
    if (localOffset + 4 <= componentData.size() && localOffset + 4 <= part_overall_size)
    { // Check against part_overall_size for logical end too
        pad_size_len = readLE<uint32_t>(&componentData[localOffset]);
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cout << "[PcbLoader LOG] pad_size_len: " << pad_size_len << " at offset " << localOffset << std::endl;
#endif
    }
    else
    {
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cerr << "[PcbLoader LOG] Component parsing: Not enough data or past part_overall_size for pad_size_len. localOffset=" << localOffset << ", part_overall_size=" << part_overall_size << ", componentData.size()=" << componentData.size() << std::endl;
#endif
        // Potentially return, but let's see if sub-blocks can still be parsed if part_overall_size is small but sub-blocks exist
    }
    localOffset += 4;

    std::string comp_footprint_name_str;
    if (pad_size_len > 0)
    {
        if (localOffset + pad_size_len <= componentData.size() && localOffset + pad_size_len <= part_overall_size)
        { // Check against part_overall_size
            comp_footprint_name_str = readCB2312String(&componentData[localOffset], pad_size_len);
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG] comp_footprint_name_str: \"" << comp_footprint_name_str << "\" at offset " << localOffset << std::endl;
#endif
        }
        else
        {
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cerr << "[PcbLoader LOG] Component parsing: Not enough data or past part_overall_size for footprint name string. localOffset=" << localOffset << ", pad_size_len=" << pad_size_len << ", part_overall_size=" << part_overall_size << ", componentData.size()=" << componentData.size() << std::endl;
#endif
            // Don't return, allow parsing of sub-blocks to continue if possible
        }
    }
    localOffset += pad_size_len;
#ifdef ENABLE_PCB_LOADER_LOGGING
    std::cout << "[PcbLoader LOG] After footprint name. localOffset: " << localOffset << std::endl;
#endif

    Component comp(comp_footprint_name_str, "", part_x, part_y);
    comp.footprint_name = comp_footprint_name_str;

#ifdef ENABLE_PCB_LOADER_LOGGING
    std::cout << "[PcbLoader LOG] Starting sub-block parsing loop. Initial localOffset=" << localOffset << ", part_overall_size=" << part_overall_size << std::endl;
#endif

    while (localOffset < part_overall_size && localOffset < componentData.size())
    {
        if (componentData.size() < localOffset + 1)
        {
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cerr << "[PcbLoader LOG] Sub-block loop: Not enough data for subType. localOffset=" << localOffset << ", componentData.size()=" << componentData.size() << std::endl;
#endif
            break;
        }
        uint8_t subType = static_cast<uint8_t>(componentData[localOffset++]);
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cout << "[PcbLoader LOG] Loop iter: localOffset after type read = " << localOffset << ", Current subType=0x" << std::hex << static_cast<int>(subType) << std::dec << std::endl;
#endif

        if (subType == 0x00)
        {
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]   Encountered subType 0x00 (End of component marker). Breaking sub-block loop." << std::endl;
#endif
            break;
        }
        if (localOffset + 4 > componentData.size() || localOffset + 4 > part_overall_size)
        {
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cerr << "[PcbLoader LOG]   Not enough data for subBlockSize. localOffset=" << localOffset << ", part_overall_size=" << part_overall_size << ", componentData.size()=" << componentData.size() << std::endl;
#endif
            break;
        }

        uint32_t subBlockSize = readLE<uint32_t>(&componentData[localOffset]);
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cout << "[PcbLoader LOG]     subBlockSize=" << subBlockSize << " (read at offset " << localOffset << ").";
#endif
        localOffset += 4;

        if (localOffset + subBlockSize > componentData.size())
        {
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cerr << "[PcbLoader LOG]     subBlock (type 0x" << std::hex << static_cast<int>(subType) << std::dec << ", size " << subBlockSize
                      << ") would exceed componentData bounds (localOffset=" << localOffset << ", componentData.size()=" << componentData.size() << "). Breaking." << std::endl;
#endif
            break;
        }
        // This check might be more relevant: ensure current offset for sub-block data isn't past the declared overall size
        if (localOffset > part_overall_size)
        {
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cerr << "[PcbLoader LOG]     localOffset (" << localOffset << ") for sub-block data start has passed part_overall_size (" << part_overall_size << "). Breaking." << std::endl;
#endif
            break;
        }

        const char *subBlockDataStart = &componentData[localOffset];
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cout << "[PcbLoader LOG]     Processing subType 0x" << std::hex << static_cast<int>(subType) << std::dec << " of size " << subBlockSize << ". Data starts at componentData[" << localOffset << "]";
#endif

        switch (subType)
        {
        case 0x05:
        { // Line Segment (part_sub_type_05)
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]       Parsing Line Segment (0x05), subBlockSize=" << subBlockSize << std::endl;
#endif
            if (subBlockSize < 24)
            {
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cerr << "[PcbLoader LOG]         Line Segment block too small: " << subBlockSize << ", expected 24. Skipping." << std::endl;
#endif
                break;
            }
            // Check if reading fields will go out of subBlockSize bounds
            if (24 > subBlockSize)
            { // Should be caught by above, but explicit check before reads
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cerr << "[PcbLoader LOG]         Logical error: subBlockSize " << subBlockSize << " < 24 for Line Segment, but initial check passed? Skipping." << std::endl;
#endif
                break;
            }
            int seg_layer = static_cast<int>(readLE<uint32_t>(subBlockDataStart));
            double x1 = static_cast<double>(readLE<int32_t>(subBlockDataStart + 4)) / xyscale;
            double y1 = static_cast<double>(readLE<int32_t>(subBlockDataStart + 8)) / xyscale;
            double x2 = static_cast<double>(readLE<int32_t>(subBlockDataStart + 12)) / xyscale;
            double y2 = static_cast<double>(readLE<int32_t>(subBlockDataStart + 16)) / xyscale;
            double thickness = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 20)) / xyscale; // Scale

#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]         Segment: (" << x1 << "," << y1 << ") to (" << x2 << "," << y2
                      << "), Thickness: " << thickness << ", Layer: " << seg_layer << std::endl;
#endif
            LineSegment seg{{x1, y1}, {x2, y2}, thickness, seg_layer};
            comp.graphical_elements.push_back(seg);
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]         Added graphical element. Count: " << comp.graphical_elements.size() << std::endl;
#endif
            break;
        }
        case 0x06:
        { // Text Label (part_sub_type_06)
            // Min size: layer(4)x(4)y(4)fontSize(4)fontScale(4)padding(4)visible(1)ps06flag(1)nameSizeField(4) = 30. Text itself is extra.
            if (subBlockSize < 30)
            {
                break;
            }
            int lbl_layer = static_cast<int>(readLE<uint32_t>(subBlockDataStart + 0));
            double lbl_x = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 4));
            double lbl_y = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 8));
            double lbl_font_size = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 12));
            double lbl_font_scale = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 16));
            // offset +20 is 4 bytes padding
            bool visible = (static_cast<uint8_t>(subBlockDataStart[24]) == 0x02);
            int ps06_flag = static_cast<uint8_t>(subBlockDataStart[25]);

            uint32_t nameSize = 0;
            if (subBlockSize >= 30)
            { // Enough for nameSize field itself
                nameSize = readLE<uint32_t>(subBlockDataStart + 26);
            }
            else
            {
                break;
            }

            std::string lbl_text;
            if (nameSize > 0 && 30 + nameSize <= subBlockSize)
            { // Check if text fits in subBlock
                lbl_text = readCB2312String(subBlockDataStart + 30, nameSize);
            }
            else if (nameSize > 0)
            { /* Declared nameSize but not enough space */
                break;
            }

            TextLabel label(lbl_text, lbl_x, lbl_y, lbl_layer, lbl_font_size);
            label.scale = lbl_font_scale;
            label.is_visible = visible;
            label.ps06_flag = ps06_flag;
            comp.text_labels.push_back(label);

            // Often, the first text label is the reference designator, second is value.
            if (comp.text_labels.size() == 1)
                comp.reference_designator = lbl_text;
            if (comp.text_labels.size() == 2)
                comp.value = lbl_text;

            break;
        }
        case 0x09:
        { // Pin (part_sub_type_09)
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]       Parsing Pin (0x09), subBlockSize=" << subBlockSize << std::endl;
#endif
            if (subBlockSize < 20 + 4)
            {
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cerr << "[PcbLoader LOG]         Pin block too small for basic fields (need 24): " << subBlockSize << ". Skipping." << std::endl;
#endif
                break;
            }

            uint32_t pin_ptr = 0; // Relative to subBlockDataStart
            // Initial padding
            if (pin_ptr + 4 > subBlockSize)
            {
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cerr << "[PcbLoader LOG] Pin too short for initial padding " << std::endl;
#endif
                break;
            }
            pin_ptr += 4;
            // std::cout << "[PcbLoader LOG]         pin_ptr after initial pad: " << pin_ptr << std::endl; // Logged below

            // Pin X
            if (pin_ptr + 4 > subBlockSize)
            {
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cerr << "[PcbLoader LOG] Pin too short for X coord " << std::endl;
#endif
                break;
            }
            double pin_x = static_cast<double>(readLE<int32_t>(subBlockDataStart + pin_ptr)) / xyscale;
            pin_ptr += 4;
            // std::cout << "[PcbLoader LOG]         pin_x: " << pin_x << ", pin_ptr: " << pin_ptr << std::endl;

            // Pin Y
            if (pin_ptr + 4 > subBlockSize)
            {
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cerr << "[PcbLoader LOG] Pin too short for Y coord " << std::endl;
#endif
                break;
            }
            double pin_y = static_cast<double>(readLE<int32_t>(subBlockDataStart + pin_ptr)) / xyscale;
            pin_ptr += 4;
            // std::cout << "[PcbLoader LOG]         pin_y: " << pin_y << ", pin_ptr: " << pin_ptr << std::endl;

            // Two 4-byte padding fields
            if (pin_ptr + 8 > subBlockSize)
            {
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cerr << "[PcbLoader LOG] Pin too short for 8b padding " << std::endl;
#endif
                break;
            }
            pin_ptr += 8;
            // std::cout << "[PcbLoader LOG]         pin_ptr after 8b padding: " << pin_ptr << std::endl;

            // Pin name size
            if (pin_ptr + 4 > subBlockSize)
            {
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cerr << "[PcbLoader LOG] Pin too short for name_size " << std::endl;
#endif
                break;
            }
            uint32_t pin_name_size = readLE<uint32_t>(subBlockDataStart + pin_ptr);
            pin_ptr += 4;
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]         Pin Coords: (" << pin_x << "," << pin_y << "), NameSize: " << pin_name_size
                      << ". pin_ptr after name_size: " << pin_ptr << std::endl;
#endif

            std::string pin_name_str; // Renamed from pin_name to avoid conflict with Pin member
            if (pin_name_size > 0)
            {
                if (pin_ptr + pin_name_size <= subBlockSize)
                { // Check against subBlockSize
                    pin_name_str = readCB2312String(subBlockDataStart + pin_ptr, pin_name_size);
#ifdef ENABLE_PCB_LOADER_LOGGING
                    std::cout << "[PcbLoader LOG]         Pin Name: \"" << pin_name_str << "\"" << std::endl;
#endif
                }
                else
                {
#ifdef ENABLE_PCB_LOADER_LOGGING
                    std::cerr << "[PcbLoader LOG]         Pin name data (size " << pin_name_size << ") extends beyond sub-block (size " << subBlockSize << ", pin_ptr " << pin_ptr << "). Skipping name." << std::endl;
#endif
                    // pin_name_str remains empty
                }
            }
            else
            {
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cout << "[PcbLoader LOG]         Pin has no name (name_size is 0)." << std::endl;
#endif
            }
            pin_ptr += pin_name_size;
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]         pin_ptr after name read/skip: " << pin_ptr << std::endl;
#endif

            // Create Pin with a default shape initially. It will be overwritten by parsed outline.
            // The Pin constructor in Pin.hpp is: Pin(double x, double y, const std::string& name, PadShape shape)
            Pin pin(pin_x, pin_y, pin_name_str, CirclePad{0.1}); // Default to a small circle.

#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]         Starting Pin Outline parsing. pin_ptr=" << pin_ptr << std::endl;
#endif
            bool first_outline_processed = false;
            for (int i = 0; i < 4; ++i)
            {
                if (pin_ptr + 5 > subBlockSize)
                {
#ifdef ENABLE_PCB_LOADER_LOGGING
                    std::cout << "[PcbLoader LOG]           Not enough data for outline end marker check (need 5, have " << (subBlockSize - pin_ptr) << "). Assuming end of outlines." << std::endl;
#endif
                    break;
                }

                bool isEndMarker = true;
                for (int k = 0; k < 5; ++k)
                {
                    if (static_cast<uint8_t>(subBlockDataStart[pin_ptr + k]) != 0)
                    {
                        isEndMarker = false;
                        break;
                    }
                }

                if (isEndMarker)
                {
#ifdef ENABLE_PCB_LOADER_LOGGING
                    std::cout << "[PcbLoader LOG]           Found 5-byte outline end marker at pin_ptr=" << pin_ptr << ". Advancing by 5." << std::endl;
#endif
                    pin_ptr += 5;
                    break;
                }

                if (pin_ptr + 9 > subBlockSize)
                {
#ifdef ENABLE_PCB_LOADER_LOGGING
                    std::cerr << "[PcbLoader LOG]           Not enough data for a pin outline (need 9, have " << (subBlockSize - pin_ptr) << "). pin_ptr=" << pin_ptr << std::endl;
#endif
                    break;
                }

                // Read raw outline data
                double outline_width_raw = static_cast<double>(readLE<uint32_t>(subBlockDataStart + pin_ptr)) / xyscale;
                pin_ptr += 4;
                double outline_height_raw = static_cast<double>(readLE<uint32_t>(subBlockDataStart + pin_ptr)) / xyscale;
                pin_ptr += 4;
                uint8_t outline_type_raw = static_cast<uint8_t>(subBlockDataStart[pin_ptr++]);

#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cout << "[PcbLoader LOG]           Parsed Outline " << i << ": W=" << outline_width_raw << ", H=" << outline_height_raw
                          << ", Type=0x" << std::hex << static_cast<int>(outline_type_raw) << std::dec
                          << ". pin_ptr after outline: " << pin_ptr << std::endl;
#endif

                if (!first_outline_processed)
                { // Only use the first outline to define the primary PadShape
                    if (outline_type_raw == 0x01)
                    { // Circular or Capsule
                        if (outline_width_raw == outline_height_raw)
                        {                                                                 // Circle
                            pin.pad_shape = CirclePad{0.0, 0.0, outline_width_raw / 2.0}; // Assuming radius from width/2
                        }
                        else
                        { // Capsule
                            // For CapsulePad, width is total width, height is diameter/rect height
                            pin.pad_shape = CapsulePad{0.0, 0.0, outline_width_raw, outline_height_raw};
                        }
                    }
                    else if (outline_type_raw == 0x02)
                    { // Rectangular or Square
                        pin.pad_shape = RectanglePad{0.0, 0.0, outline_width_raw, outline_height_raw};
                    }
                    else
                    {
#ifdef ENABLE_PCB_LOADER_LOGGING
                        std::cout << "[PcbLoader LOG]           Unknown outline type for primary shape: 0x" << std::hex << static_cast<int>(outline_type_raw) << std::dec << ". Keeping default pin shape." << std::endl;
#endif
                        // pin.pad_shape remains the default CirclePad{0.1}
                    }
                    first_outline_processed = true;
#ifdef ENABLE_PCB_LOADER_LOGGING
                    // Logging the determined shape type
                    size_t shape_idx = pin.pad_shape.index();
                    std::string shape_name = "Unknown";
                    if (shape_idx == 0)
                        shape_name = "CirclePad";
                    else if (shape_idx == 1)
                        shape_name = "RectanglePad";
                    else if (shape_idx == 2)
                        shape_name = "CapsulePad";
                    std::cout << "[PcbLoader LOG]           Pin primary shape set to " << shape_name << " (W:" << outline_width_raw << " H:" << outline_height_raw << ") from first outline." << std::endl;
#endif
                }
                // The Pin class in Pin.hpp does not have a std::vector<PinOutline> outlines;
                // nor does it have direct width/height members.
                // If all outlines need to be stored, Pin.hpp would need modification.
                // For now, we just use the first outline to define the pad_shape.
            }
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]         Finished Pin Outline parsing. pin_ptr=" << pin_ptr << std::endl;
#endif

            // Net ID is 12 bytes from the end of the pin sub-block.
            // (net_index 4 bytes + final padding 8 bytes = 12 bytes total for footer)
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]         Checking for Net ID. subBlockSize=" << subBlockSize << ", pin_ptr=" << pin_ptr << std::endl;
#endif
            uint32_t net_id_footer_size = 12;
            if (subBlockSize >= net_id_footer_size && subBlockSize >= pin_ptr)
            { // Ensure subBlock is large enough for footer AND pin_ptr hasn't gone past subBlockSize
                // Check if current pin_ptr position allows for a 12-byte read if net_id was immediately next, OR if it's at a fixed end.
                // The original logic: subBlockSize >= pin_ptr + 12 (meaning 12 bytes *after* current ptr)
                // AND subBlockSize >= 12 (absolute check)
                // Then read from subBlockDataStart + subBlockSize - 12.
                // This implies net_id is always at a fixed position from the end.
                if (subBlockSize >= net_id_footer_size)
                { // Check if block is big enough to contain the footer at all
                    pin.net_id = static_cast<int>(readLE<uint32_t>(subBlockDataStart + subBlockSize - net_id_footer_size));
#ifdef ENABLE_PCB_LOADER_LOGGING
                    std::cout << "[PcbLoader LOG]         Pin Net ID: " << pin.net_id << " (read from offset subBlockSize - 12)" << std::endl;
#endif
                }
                else
                {
#ifdef ENABLE_PCB_LOADER_LOGGING
                    std::cerr << "[PcbLoader LOG]         Pin subBlockSize (" << subBlockSize << ") too small to read net_id from subBlockSize - 12." << std::endl;
#endif
                    pin.net_id = -1;
                }
            }
            else
            {
#ifdef ENABLE_PCB_LOADER_LOGGING
                std::cerr << "[PcbLoader LOG]         Not enough data for Net ID relative to pin_ptr or subBlockSize too small. "
                          << "subBlockSize=" << subBlockSize << ", pin_ptr=" << pin_ptr
                          << ". Remaining from pin_ptr: " << (subBlockSize > pin_ptr ? subBlockSize - pin_ptr : 0) << std::endl;
#endif
                pin.net_id = -1;
            }

            // Diode readings association
            if (diodeReadingsType_ == 1 && !comp.reference_designator.empty() && !pin.pin_name.empty())
            {
                auto comp_it = diodeReadings_.find(comp.reference_designator);
                if (comp_it != diodeReadings_.end())
                {
                    auto pin_it = comp_it->second.find(pin.pin_name);
                    if (pin_it != comp_it->second.end())
                    {
                        pin.diode_reading = pin_it->second;
#ifdef ENABLE_PCB_LOADER_LOGGING
                        std::cout << "[PcbLoader LOG]         Associated diode reading (Type 1): " << pin.diode_reading << std::endl;
#endif
                    }
                }
            }
            else if (diodeReadingsType_ == 2 && pin.net_id != -1)
            {
                // This requires nets to be parsed first or a two-pass approach for diode readings.
                // For now, this part of diode reading might not work perfectly if nets aren't ready.
                // A placeholder for net name lookup:
                // const Net* net = board.getNetById(pin.net_id);
                // if (net && diodeReadings_.count(net->name) && diodeReadings_.at(net->name).count("0")) {
                //    pin.diode_reading = diodeReadings_.at(net->name).at("0");
                // }
            }

            comp.pins.push_back(pin);
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]         Added pin. Total pins for \"" << comp.reference_designator << "\": " << comp.pins.size() << std::endl;
#endif
            break;
        }
        default: // Unknown sub-type
#ifdef ENABLE_PCB_LOADER_LOGGING
            std::cout << "[PcbLoader LOG]       Unknown or unhandled component subType: 0x" << std::hex << static_cast<int>(subType) << std::dec << " of size " << subBlockSize << std::endl;
#endif
            break;
        }
        localOffset += subBlockSize; // Advance by the size of the sub-block we just processed
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cout << "[PcbLoader LOG]   End of sub-block processing for type 0x" << std::hex << static_cast<int>(subType) << std::dec << ". Advanced localOffset by subBlockSize (" << subBlockSize << "). New localOffset=" << localOffset << std::endl;
#endif
    }
#ifdef ENABLE_PCB_LOADER_LOGGING
    std::cout << "[PcbLoader LOG] Finished sub-block parsing loop. final localOffset=" << localOffset << ", part_overall_size=" << part_overall_size << std::endl;
#endif
    if (localOffset > part_overall_size && part_overall_size > 0)
    {
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cerr << "[PcbLoader LOG] Warning: localOffset (" << localOffset << ") exceeded part_overall_size (" << part_overall_size << ")" << std::endl;
#endif
    }
#ifdef ENABLE_PCB_LOADER_LOGGING
    if (comp.pins.empty() && (!comp.graphical_elements.empty() || !comp.text_labels.empty()))
    {
        std::cout << "[PcbLoader LOG] Component \"" << comp.reference_designator << "\" (Footprint: \"" << comp.footprint_name << "\") has "
                  << comp.graphical_elements.size() << " segments, " << comp.text_labels.size() << " labels, but NO pins." << std::endl;
    }
#endif

    // If component reference designator is still not set (e.g. from type 0x06 text label), try to use footprint name or make a generic one.
    if (comp.reference_designator.empty())
    {
        if (!comp.footprint_name.empty())
        {
            comp.reference_designator = comp.footprint_name + "?"; // e.g. "0805?"
        }
        else
        {
            static int unnamed_comp_counter = 0;
            std::ostringstream ss;
            ss << "COMP?" << unnamed_comp_counter++;
            comp.reference_designator = ss.str();
        }
#ifdef ENABLE_PCB_LOADER_LOGGING
        std::cout << "[PcbLoader LOG] Component had no refdes from labels, assigned: " << comp.reference_designator << std::endl;
#endif
    }

#ifdef ENABLE_PCB_LOADER_LOGGING
    std::cout << "[PcbLoader LOG] ----- End Component Parse: " << comp.reference_designator
              << " (Footprint: " << comp.footprint_name << ")"
              << ", Pins: " << comp.pins.size()
              << ", Segments: " << comp.graphical_elements.size()
              << ", Labels: " << comp.text_labels.size() << " -----" << std::endl;
#endif

    // DO STUFF AFTER PARSING BEFORE ADDING TO BOARD
    // Calculate component width and height and rotation
    // We need to ensure we actually calculate the center of the component, by getting the average of themin and max x and y coordinates of the component.
    // We will need to iterate through the graphical elements and to do this.

    if (!comp.graphical_elements.empty())
    {
        double min_coord_x = std::numeric_limits<double>::max();
        double max_coord_x = std::numeric_limits<double>::lowest();
        double min_coord_y = std::numeric_limits<double>::max();
        double max_coord_y = std::numeric_limits<double>::lowest();

        for (const auto &seg : comp.graphical_elements)
        {
            min_coord_x = std::min({min_coord_x, seg.start.x, seg.end.x});
            max_coord_x = std::max({max_coord_x, seg.start.x, seg.end.x});
            min_coord_y = std::min({min_coord_y, seg.start.y, seg.end.y});
            max_coord_y = std::max({max_coord_y, seg.start.y, seg.end.y});
        }

        // Check if any coordinates were actually processed (i.e., bounds are not still at initial extreme values)
        if (max_coord_x >= min_coord_x && max_coord_y >= min_coord_y)
        {
            comp.width = max_coord_x - min_coord_x;
            comp.height = max_coord_y - min_coord_y;
            // Optionally, update component center if your Component struct uses it
            // comp.x_coord = (min_coord_x + max_coord_x) / 2.0; // Assuming x_coord is center
            // comp.y_coord = (min_coord_y + max_coord_y) / 2.0; // Assuming y_coord is center
        }
        // If comp.width/height were pre-filled from footprint data and graphical_elements are very small or missing,
        // you might choose to keep the pre-filled ones or decide on a priority.
        // For now, this will overwrite them if graphical_elements exist and form a valid bbox.
    }
    // If graphical_elements is empty, comp.width and comp.height retain any values they had from earlier parsing stages.

    board.addComponent(comp);
}

bool PcbLoader::parsePostV6Block(const std::vector<char> &fileData, Board &board,
                                 std::vector<char>::const_iterator v6_iter)
{
    // Logic adapted from XZZPCBLoader::parsePostV6Block
    // v6_iter points to the start of the "v6v6555v6v6" marker.
    // The actual data starts after this marker (11 bytes) and potentially some more skips.

    uint32_t currentFileOffset = static_cast<uint32_t>(std::distance(fileData.cbegin(), v6_iter)) + 11; // Skip the marker itself

    if (currentFileOffset >= fileData.size())
        return false;

    // Original logic had a fixed skip of 7 bytes, then checked fileData[currentFileOffset]
    // Let's see if we can determine separator more dynamically based on the .hexpat approach for post_v6
    // The .hexpat's find_seperator() and related logic is complex to replicate directly here without its full context.
    // The old XZZPCBLoader had direct checks for 0x0A or 0x0D after skipping 7 bytes.

    currentFileOffset += 7; // Skip 7 bytes as in original loader
    if (currentFileOffset >= fileData.size())
        return false;

    try
    {
        if (static_cast<uint8_t>(fileData[currentFileOffset]) == 0x0A)
        {
            // Type 1 diode readings (from old loader comments)
            // Format: 0x0A '=VOLTAGE=PART_NAME(PIN_NAME)'
            diodeReadingsType_ = 1;
            while (currentFileOffset < fileData.size())
            {
                currentFileOffset += 1; // Skip 0x0A
                if (currentFileOffset >= fileData.size() || static_cast<uint8_t>(fileData[currentFileOffset]) != '=')
                    break;
                currentFileOffset += 1; // Skip '='

                std::string voltageReading, partName, pinName;
                uint32_t startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != '=')
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break;
                voltageReading = readCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++; // Skip '='

                startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != '(')
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break;
                partName = readCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++; // Skip '('

                startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != ')')
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break;
                pinName = readCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++; // Skip ')'

                diodeReadings_[partName][pinName] = voltageReading;
            }
        }
        else
        {
            // Type 2 or Type 3 diode readings (from old loader comments)
            // Type 2 started with CD BC 0D 0A, then 'NET_NAME=VALUE'
            // Type 3 started with 0D 0A, then 'NET_NAME=VALUE'
            // The old code checked `fileData[currentPointer] != 0x0D` before the +=2 for Type 2, implying Type 2 might have an extra initial skip.
            if (static_cast<uint8_t>(fileData[currentFileOffset]) != 0x0D)
            {
                currentFileOffset += 2; // Skip two unknown bytes for Type 2 (CD BC)
            }
            diodeReadingsType_ = 2;

            while (currentFileOffset < fileData.size())
            {
                if (currentFileOffset + 2 > fileData.size())
                    break;
                if (static_cast<uint8_t>(fileData[currentFileOffset]) != 0x0D || static_cast<uint8_t>(fileData[currentFileOffset + 1]) != 0x0A)
                    break;              // Expect 0D 0A separator
                currentFileOffset += 2; // Skip 0x0D 0x0A

                if (currentFileOffset >= fileData.size())
                    break;
                // Check for double 0D0A which might signify end of this section
                if (currentFileOffset + 2 <= fileData.size() &&
                    static_cast<uint8_t>(fileData[currentFileOffset]) == 0x0D &&
                    static_cast<uint8_t>(fileData[currentFileOffset + 1]) == 0x0A)
                {
                    break;
                }

                std::string netName, value;
                uint32_t startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != '=')
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break;
                netName = readCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++; // Skip '='

                startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != 0x0D)
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break; // Value should end before 0D (start of next 0D 0A)
                value = readCB2312String(&fileData[startPos], currentFileOffset - startPos);

                diodeReadings_[netName]["0"] = value; // Use "0" as pin key for this type
            }
        }
        return true;
    }
    catch (const std::runtime_error &e)
    {
        // Log e.what()
        return false;
    }
}

bool PcbLoader::parseNetBlock(const std::vector<char> &fileData, Board &board, uint32_t netDataOffset)
{
    if (netDataOffset == 0 || netDataOffset >= fileData.size())
    {
        return true; // No net block or invalid offset, considered okay.
    }

    try
    {
        // Net block starts with its total size (uint32_t)
        if (netDataOffset + 4 > fileData.size())
        {
            // Not enough data even for the block size itself
            return false;
        }
        uint32_t netBlockTotalSize = readLE<uint32_t>(&fileData[netDataOffset]);

        uint32_t currentRelativeOffset = 4;                 // Start reading after the netBlockTotalSize field
        uint32_t netBlockEndOffset = 4 + netBlockTotalSize; // Relative end offset within the conceptual net block data

        while (currentRelativeOffset < netBlockEndOffset)
        {
            // Each net entry: net_record_size (u32), net_id (u32), net_name (char[net_record_size - 8])
            if (netDataOffset + currentRelativeOffset + 8 > fileData.size())
            {
                // Not enough data for net_record_size and net_id fields
                // This indicates a malformed block or premature end.
                return false;
            }

            uint32_t netRecordSize = readLE<uint32_t>(&fileData[netDataOffset + currentRelativeOffset]);
            uint32_t netId = readLE<uint32_t>(&fileData[netDataOffset + currentRelativeOffset + 4]);

            if (netRecordSize < 8)
            {
                // Record size must be at least 8 to hold its own size and the ID.
                // If smaller, it's corrupt. Advance past this problematic record as declared.
                // Log_Warning("Invalid net record size %u at offset %u, skipping", netRecordSize, netDataOffset + currentRelativeOffset);
                if (netRecordSize == 0)
                    return false; // Avoid infinite loop if netRecordSize is 0
                currentRelativeOffset += netRecordSize;
                continue;
            }

            uint32_t netNameLength = netRecordSize - 8;
            if (netDataOffset + currentRelativeOffset + 8 + netNameLength > fileData.size() || 8 + netNameLength > netRecordSize)
            {
                // Net name would extend beyond file data, or netRecordSize is inconsistent.
                // Log_Error("Net record for ID %u at offset %u claims size %u but data is insufficient or inconsistent.", netId, netDataOffset + currentRelativeOffset, netRecordSize);
                return false;
            }

            std::string netName;
            if (netNameLength > 0)
            {
                netName = readCB2312String(&fileData[netDataOffset + currentRelativeOffset + 8], netNameLength);
            }

            // Use emplace to construct Net in place, avoiding issues with default constructor
            board.nets.emplace(static_cast<int>(netId), Net(static_cast<int>(netId), netName));

            currentRelativeOffset += netRecordSize;
        }

        // Check if we consumed exactly the amount of data specified by netBlockTotalSize
        if (currentRelativeOffset != netBlockEndOffset)
        {
            // Log_Warning("Net block parsing finished, but read %u bytes, expected %u based on netBlockTotalSize.", currentRelativeOffset - 4, netBlockTotalSize);
            // This might indicate an issue with the last record or the total size field.
        }

        return true;
    }
    catch (const std::runtime_error &e)
    {
        // Log_Exception(e, "Exception during net block parsing.");
        return false;
    }
}

// Implementations for parsePostV6Block, and element-specific parsers (parseArc, parseVia, etc.) will follow.
// ... (make sure this is placed correctly relative to other definitions) ...
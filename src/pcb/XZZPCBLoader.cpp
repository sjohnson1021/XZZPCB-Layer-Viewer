#include "XZZPCBLoader.hpp"

#include <algorithm>  // For std::search
#include <cstdint>
#include <cstring>  // For std::memcpy
#include <fstream>
#include <iomanip>   // For std::setw, std::setfill, std::hex
#include <iostream>  // Added for logging
#include <limits>    // For std::numeric_limits
#include <sstream>   // For std::ostringstream
#include <stdexcept>
#include <string>
#include <variant>  // Required for std::visit or index() on PadShape
#include <vector>

#include "Board.hpp"

#include "../utils/ColorUtils.hpp"  // Added for layer color generation
#include "../utils/des.h"
#include "processing/PinResolver.hpp"

// Define this to enable verbose logging for PcbLoader (disabled for performance)
// #define ENABLE_PCB_LOADER_LOGGING

// Static data for DES key generation, similar to old XZZXZZPCBLoader.cpp
static const unsigned char hexconv[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6,  7,  8,  9,  0,  0,  0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const std::vector<uint16_t> des_key_byte_list = {0xE0, 0xCF, 0x2E, 0x9F, 0x3C, 0x33, 0x3C, 0x33};

// This is the scale factor for the XZZ files.
static const int xyscale = 10000;
// Implementation of DefineStandardLayers
void PcbLoader::DefineStandardLayers(Board& board)
{
    // Clear any existing layers first, in case this is a reload or reused Board object
    board.layers.clear();

    // Define layer for pins
	board.AddLayer(Board::LayerInfo(Board::kBottomCompLayer, "Components", Board::LayerInfo::LayerType::kSignal));
    board.AddLayer(Board::LayerInfo(Board::kBottomPinsLayer, "Pins", Board::LayerInfo::LayerType::kSignal));
    board.AddLayer(Board::LayerInfo(Board::kTopCompLayer, "Components", Board::LayerInfo::LayerType::kSignal));
    board.AddLayer(Board::LayerInfo(Board::kTopPinsLayer, "Pins", Board::LayerInfo::LayerType::kSignal));
    board.AddLayer(Board::LayerInfo(Board::kViasLayer, "Vias", Board::LayerInfo::LayerType::kSignal));
    // Define Trace Layers (1-16)
    for (int i = 1; i <= 16; ++i) {
        std::string name = "Trace Layer " + std::to_string(i);
        // Layer 16 is often special (e.g., last layer in a sequence)
        // For now, all are Signal type. Specific XZZ usage might refine this.
        board.AddLayer(Board::LayerInfo(i, name, Board::LayerInfo::LayerType::kSignal));
    }

    // Silkscreen (17)
    board.AddLayer(Board::LayerInfo(Board::kSilkscreenLayer, "Silkscreen", Board::LayerInfo::LayerType::kSilkscreen));

    // Unknown Layers (18-27) - treat as generic 'Other' or 'Comment' type for now
    for (int i = 18; i <= 27; ++i) {
        std::string name = "Unknown Layer " + std::to_string(i);
        // These might be used for internal notes, mechanical details, or additional silkscreen
        // Defaulting to 'Other' type. Users can inspect content.
        board.AddLayer(Board::LayerInfo(i, name, Board::LayerInfo::LayerType::kOther));
    }

    // Board Edges (28)
    board.AddLayer(Board::LayerInfo(Board::kBoardEdgesLayer, "Board Edges", Board::LayerInfo::LayerType::kBoardOutline));

    // After adding all layers, if your Board class has a function to assign unique colors:
    // board.RegenerateLayerColors(); // Or similar, if it exists in Board.hpp
}

std::unique_ptr<Board> PcbLoader::LoadFromFile(const std::string& filePath)
{
    std::vector<char> fileData;
    if (!ReadFileData(filePath, fileData)) {
        // Consider logging an error here
        return nullptr;
    }

    if (!VerifyFormat(fileData)) {
        // Consider logging an error here
        return nullptr;
    }

    if (!DecryptFileDataIfNeeded(fileData)) {
        // Consider logging an error here
        return nullptr;
    }

    auto board = std::make_unique<Board>();
    board->file_path = filePath;
    // board->board_name = extract from file path or header?
    DefineStandardLayers(*board);

    uint32_t mainDataOffset = 0;
    uint32_t netDataOffset = 0;
    uint32_t imageDataOffset = 0;  // Currently unused by parser logic beyond header
    uint32_t mainDataBlocksSize = 0;

    if (!ParseHeader(fileData, *board, mainDataOffset, netDataOffset, imageDataOffset, mainDataBlocksSize)) {
        return nullptr;  // Error parsing header
    }

    if (!ParseMainDataBlocks(fileData, *board, mainDataOffset, mainDataBlocksSize)) {
        return nullptr;  // Error parsing main data blocks
    }

    if (!ParseNetBlock(fileData, *board, netDataOffset)) {
        return nullptr;  // Error parsing net block
    }

    // Handle post-v6 data if present (diode readings)
    static const std::vector<uint8_t> v6_marker = {0x76, 0x36, 0x76, 0x36, 0x35, 0x35, 0x35, 0x76, 0x36, 0x76, 0x36};
    auto v6_iter = std::search(fileData.cbegin(), fileData.cend(), v6_marker.cbegin(), v6_marker.cend());
    if (v6_iter != fileData.cend()) {
        if (!ParsePostV6Block(fileData, *board, v6_iter)) {
            // Optional: log error but continue, as this might be auxiliary data
        }
    }

    // Board is populated. Now calculate its bounds and normalize coordinates.
    BLRect original_extents = board->GetBoundingBox(true);  // Use layer 28 traces, include even if layer 28 is initially hidden

    if (original_extents.w > 0 || original_extents.h > 0) {  // Allow for 1D lines to still have a center
        board->origin_offset = board->NormalizeCoordinatesAndGetCenterOffset(original_extents);
        // Store the calculated dimensions on the board object itself
        board->width = original_extents.w;
        board->height = original_extents.h;
    } else {
        // If no valid outline on layer 28 (or board is empty/point-like),
        // dimensions remain 0 or default. Normalization is skipped.
        // board->width and board->height will retain their initial values (likely 0.0).
        // Optionally, log a warning if an outline was expected for an XZZ file.
        // std::cerr << "Warning: Could not determine board bounds from layer 28 outline." << std::endl;
    }

    // The Board::Board(filePath) constructor is responsible for setting m_isLoaded.
    // By reaching this point in the loader, we assume the loading process itself was successful
    // in terms of reading and parsing data into the board structure.

    // Apply global coordinate mirroring to correct coordinate system mismatch
    // This is a static transformation applied at load time, separate from interactive transformations
    ApplyGlobalCoordinateMirroring(*board);

    // Pin orientations are now read directly from PCB files as rotation data
    // No need for heuristic orientation processing
    return board;
}

bool PcbLoader::ReadFileData(const std::string& filePath, std::vector<char>& fileData)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    fileData.resize(static_cast<size_t>(fileSize));
    if (!file.read(fileData.data(), fileSize)) {
        fileData.clear();
        return false;
    }
    return true;
}

bool PcbLoader::VerifyFormat(const std::vector<char>& fileData)
{
    if (fileData.size() < 6)
        return false;
    const char* signature = "XZZPCB";
    if (std::memcmp(fileData.data(), signature, 6) == 0)
        return true;

    // Check for XORed encrypted format
    if (fileData.size() >= 0x11) {  // Need at least 0x10 + 1 byte
        uint8_t xor_key = static_cast<uint8_t>(fileData[0x10]);
        if (xor_key != 0x00) {  // If 0x00, it's not XORed this way or already decrypted
            bool match = true;
            for (int i = 0; i < 6; ++i) {
                if ((static_cast<uint8_t>(fileData[i]) ^ xor_key) != static_cast<uint8_t>(signature[i])) {
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
bool PcbLoader::DecryptFileDataIfNeeded(std::vector<char>& fileData)
{
    if (fileData.size() < 0x11 || static_cast<uint8_t>(fileData[0x10]) == 0x00) {
        return true;  // Not encrypted this way or too small
    }

    uint8_t xor_key = static_cast<uint8_t>(fileData[0x10]);

    // The v6 marker search helps to avoid XORing the post-v6 block if it exists
    // This matches the logic in the original XZZPCBLoader
    static const std::vector<uint8_t> v6_marker = {0x76, 0x36, 0x76, 0x36, 0x35, 0x35, 0x35, 0x76, 0x36, 0x76, 0x36};
    auto v6_iter = std::search(fileData.begin(), fileData.end(), v6_marker.begin(), v6_marker.end());

    size_t end_offset = (v6_iter != fileData.end()) ? std::distance(fileData.begin(), v6_iter) : fileData.size();

    for (size_t i = 0; i < end_offset; ++i) {
        fileData[i] ^= xor_key;
    }
    // Crucially, after XORing, the byte at 0x10 should become 0x00 if it was correctly decrypted.
    // This is implicitly handled as fileData[0x10] ^ xor_key will be 0 if it was xor_key.
    return true;
}

// This method is specifically for DES-decrypting component data blocks.
void PcbLoader::DecryptComponentBlock(std::vector<char>& blockData)
{
    std::ostringstream key_hex_stream;
    key_hex_stream.str().reserve(16);  // DES key is 8 bytes = 16 hex chars

    for (size_t i = 0; i < des_key_byte_list.size(); i += 2) {
        uint16_t value = (des_key_byte_list[i] << 8) | des_key_byte_list[i + 1];
        value ^= 0x3C33;  // Specific XOR for this key derivation
        key_hex_stream << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
    }
    std::string key_hex_str = key_hex_stream.str();

    uint64_t des_key = 0;
    for (int i = 0; i < 8; ++i) {
        unsigned char byte_str[3] = {static_cast<unsigned char>(key_hex_str[i * 2]), static_cast<unsigned char>(key_hex_str[i * 2 + 1]), '\0'};
        uint64_t byte_val = hexconv[byte_str[0]] * 16 + hexconv[byte_str[1]];
        des_key |= (byte_val << ((7 - i) * 8));
    }

    std::vector<char> decrypted_data;
    decrypted_data.reserve(blockData.size());

    const char* p = blockData.data();
    const char* ep = p + blockData.size();

    while (p < ep) {
        if (std::distance(p, ep) < 8)
            break;  // Not enough data for a full DES block

        uint64_t encrypted_block64 = 0;
        // Read block as big-endian for DES function
        for (int i = 0; i < 8; ++i) {
            encrypted_block64 = (encrypted_block64 << 8) | static_cast<uint8_t>(p[i]);
        }

        uint64_t decrypted_block64 = Des(encrypted_block64, des_key, 'd');

        // Write decrypted block back (DES output is big-endian)
        for (int i = 7; i >= 0; --i) {
            decrypted_data.push_back(static_cast<char>((decrypted_block64 >> (i * 8)) & 0xFF));
        }
        p += 8;
    }
    blockData = decrypted_data;  // Replace original with decrypted version
}

std::string PcbLoader::ReadCB2312String(const char* data, size_t length)
{
    std::string result;
    result.reserve(length);  // Approximate reservation
    bool last_was_high_byte = false;

    for (size_t i = 0; i < length; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < 0x80) {  // ASCII
            result += static_cast<char>(c);
            last_was_high_byte = false;
        } else {  // Potentially part of a multi-byte character (e.g., GB2312)
            // For now, treat non-ASCII as '?'
            // A proper GB2312 to UTF-8 conversion would be more robust.
            if (!last_was_high_byte) {  // First byte of a potential 2-byte char
                result += '?';
            }
            last_was_high_byte = !last_was_high_byte;
        }
    }
    return result;
}

bool PcbLoader::ParseHeader(const std::vector<char>& fileData, Board& board, uint32_t& outMainDataOffset, uint32_t& outNetDataOffset, uint32_t& outImageDataOffset, uint32_t& outMainDataBlocksSize)
{
    if (fileData.size() < 0x44) {  // Minimum size for the header fields we need
        return false;              // Error: File too small for header
    }

    try {
        // According to XZZPCB Decrypted.hexpat and old XZZXZZPCBLoader.cpp:
        // - image_block_start is at 0x24, relative to 0x20
        // - net_block_start is at 0x28, relative to 0x20
        // - main_data_blocks_size is at 0x40 (absolute address in fileData)

        // uint32_t header_addresses_size = ReadLE<uint32_t>(&fileData[0x20]); // This was in old loader, seems unused for critical offsets by old loader.

        // Aligning with XZZXZZPCBLoader.cpp: offsets are added to 0x20 unconditionally
        // if the file format implies these fields are always relative offsets stored at 0x24/0x28.
        // If an offset of 0 at 0x24 truly means "no image block and 0 is not relative to 0x20",
        // then the previous conditional logic was safer. Assuming old loader was correct:
        outImageDataOffset = ReadLE<uint32_t>(&fileData[0x24]) + 0x20;
        outNetDataOffset = ReadLE<uint32_t>(&fileData[0x28]) + 0x20;

        // The main data block content starts immediately after its size field.
        // The size field itself is at absolute offset 0x40.
        outMainDataOffset = 0x40;
        outMainDataBlocksSize = ReadLE<uint32_t>(&fileData[outMainDataOffset]);

        // Basic validation of offsets (crude check, can be improved)
        if (outMainDataOffset + 4 + outMainDataBlocksSize > fileData.size() || (outNetDataOffset > 0 && outNetDataOffset > fileData.size()) ||
            (outImageDataOffset > 0 && outImageDataOffset > fileData.size())) {
            // Potentially corrupted offsets or file too small for declared sizes
            // However, some files might have 0 for net/image offsets if blocks are absent.
            // The main block size is most critical here.
            if (outMainDataOffset + 4 + outMainDataBlocksSize > fileData.size() && outMainDataBlocksSize > 0) {
                return false;  // Main data block seems to exceed file bounds
            }
        }

        // Extract board name from file path if possible (simple version)
        size_t last_slash = board.file_path.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            board.board_name = board.file_path.substr(last_slash + 1);
        } else {
            board.board_name = board.file_path;
        }
        // Remove extension
        size_t dot_pos = board.board_name.rfind('.');
        if (dot_pos != std::string::npos) {
            board.board_name = board.board_name.substr(0, dot_pos);
        }

        return true;
    } catch (const std::exception& e) {
        // Log e.what()
        return false;
    }
}

bool PcbLoader::ParseMainDataBlocks(const std::vector<char>& fileData, Board& board, uint32_t mainDataOffset, uint32_t mainDataBlocksSize)
{
    if (mainDataBlocksSize == 0)
        return true;  // No main data to parse

    // The actual data starts after the mainDataBlocksSize field (4 bytes)
    uint32_t currentOffset = mainDataOffset + 4;
    uint32_t endOffsetOfDataRegion = currentOffset + mainDataBlocksSize;

    try {
        while (currentOffset < endOffsetOfDataRegion) {
            // Ensure there's enough data for at least a block type and size, plus potential padding.
            if (currentOffset + 5 > fileData.size() || currentOffset + 5 > endOffsetOfDataRegion) {
                // Not enough data for a full block header or past declared region end.
                // This could be trailing padding or an issue.
                // For now, we assume valid files won't have partial blocks here.
                break;
            }

            // Handle potential 4-byte null padding between blocks
            if (ReadLE<uint32_t>(&fileData[currentOffset]) == 0) {
                currentOffset += 4;
                continue;
            }

            uint8_t blockType = static_cast<uint8_t>(fileData[currentOffset]);
            currentOffset += 1;

            uint32_t blockSize = ReadLE<uint32_t>(&fileData[currentOffset]);
            currentOffset += 4;

            // Boundary check for the block content itself
            if (currentOffset + blockSize > fileData.size() || currentOffset + blockSize > endOffsetOfDataRegion) {
                // Block claims to extend beyond file data or its allocated region
                return false;  // Critical error
            }

            const char* blockDataStart = &fileData[currentOffset];

            switch (blockType) {
                case 0x01:  // Arc
                    ParseArc(blockDataStart, board);
                    break;
                case 0x02:  // Via
                    ParseVia(blockDataStart, blockSize, board);
                    break;
                case 0x03:  // Unknown type_03 from hexpat, skip for now
                    // std::cout << "Skipping unknown block type 0x03 of size " << blockSize << std::endl;
                    break;
                case 0x05:  // Trace (Line Segment in hexpat for traces)
                    ParseTrace(blockDataStart, board);
                    break;
                case 0x06:  // Text Label (standalone)
                    ParseTextLabel(blockDataStart, board, true /* isStandalone */);
                    break;
                case 0x07:  // Component
                    ParseComponent(blockDataStart, blockSize, board);
                    break;
                case 0x09:  // Test Pad / Drill Hole (type_09 in hexpat) - Treat as Via for now?
                    // Or create a new element type like 'DrillHole' or 'TestPad'
                    // For now, let's define a placeholder or skip.
                    // std::cout << "Skipping block type 0x09 (TestPad/DrillHole) of size " << blockSize << std::endl;
                    ParseVia(blockDataStart, blockSize, board);  // TEMPORARY: treat as via. TODO: Revisit this.
                    break;
                default:
                    // Unknown or unhandled block type
                    // std::cout << "Skipping unknown block type 0x" << std::hex << (int)blockType << std::dec << " of size " << blockSize << std::endl;
                    break;
            }
            currentOffset += blockSize;
        }
        return true;
    } catch (const std::runtime_error& e) {  // Catch potential errors from ReadLE
        // Log e.what()
        return false;
    }
}

void PcbLoader::ParseArc(const char* data, Board& board)
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

    int layer_id = static_cast<int>(ReadLE<uint32_t>(data));
    double cx = static_cast<double>(ReadLE<uint32_t>(data + 4)) / xyscale;
    double cy = static_cast<double>(ReadLE<uint32_t>(data + 8)) / xyscale;
    double radius = static_cast<double>(ReadLE<int32_t>(data + 12)) / xyscale;
    // Correct angle scaling: XZZ format likely stores angles scaled by 10000 (degrees * 10000)
    double start_angle = static_cast<double>(ReadLE<int32_t>(data + 16)) / 10000.0;
    double end_angle = static_cast<double>(ReadLE<int32_t>(data + 20)) / 10000.0;
    double thickness = static_cast<double>(ReadLE<int32_t>(data + 24)) / xyscale;  // Assuming 'scale' is thickness
    int net_id = static_cast<int>(ReadLE<int32_t>(data + 28));
    // int32_t unknown_arc_val = ReadLE<int32_t>(data + 32); // If needed

    Arc arc(layer_id, Vec2(cx, cy), radius, start_angle, end_angle, thickness);
    arc.thickness = thickness;
    arc.SetNetId(net_id);  // Changed from arc.net_id
    board.AddArc(arc);
}

void PcbLoader::ParseVia(const char* data, uint32_t blockSize, Board& board)
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
    double x = static_cast<double>(ReadLE<int32_t>(data)) / xyscale;
    double y = static_cast<double>(ReadLE<int32_t>(data + 4)) / xyscale;
    double radius_a = static_cast<double>(ReadLE<int32_t>(data + 8)) / xyscale;   // Pad radius on layer_a
    double radius_b = static_cast<double>(ReadLE<int32_t>(data + 12)) / xyscale;  // Pad radius on layer_b
    int layer_a = static_cast<int>(ReadLE<uint32_t>(data + 16));
    int layer_b = static_cast<int>(ReadLE<uint32_t>(data + 20));
    int net_id = static_cast<int>(ReadLE<uint32_t>(data + 24));
    uint32_t text_len = ReadLE<uint32_t>(data + 28);

    // For our Via class, we might simplify to one pad_radius if they are usually the same,
    // or keep separate if they can differ. The current Via.hpp has pad_radius_from, pad_radius_to.
    // It also has drill_diameter, which isn't directly in this old struct.
    // We'll use the larger of radius_a/radius_b for pad radii and perhaps infer drill later or set a default.
    // Provide default for drill_diameter (e.g., 0.0 or min radius), pass net_id, and empty string for text initially.
    double drill_diameter = std::min(radius_a, radius_b) * 0.6;  // Example: 60% of smaller pad as drill

    Via via(x, y, layer_a, layer_b, radius_a, radius_b, drill_diameter, net_id, "");  // Updated constructor call
    // via.net_id = net_id; // net_id is now passed in constructor. setNetId can be used if needed later.
    // via.drill_diameter = default or calculated? For now, Via.hpp has a default.

    if (text_len > 0) {
        // Validate text_len
        // Check 1: Does text_len cause read beyond current block?
        // The text itself starts at offset 32 within the block data.
        if (32 + text_len > blockSize) {
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
        if (text_len > MAX_SANE_VIA_TEXT_LEN && text_len != 0) {  // text_len might have been set to 0 by previous check
            // Log this or handle it.
            // Consider logging: fprintf(stderr, "Warning: Via text_len (%u) is unusually large (max sane: %u). Clamping text_len to 0.\n", text_len, MAX_SANE_VIA_TEXT_LEN);
            via.optional_text = "";
            text_len = 0;
        }

        if (text_len > 0) {  // Re-check after potential clamping
            // Ensure not to read past block boundary, though blockSize check in parseMainDataBlocks helps.
            // The via_text_length in hexpat for type_02 says it's typically 0 or 1, meaning a single char.
            // If it's truly a length, then data + 32 is the start of text.
            // The old XZZPCBLoader has: via.text = std::string(&fileData[currentOffset + 32], textLen);
            // So, text_len is indeed the length of the string starting at offset 32 from blockDataStart.
            via.optional_text = ReadCB2312String(data + 32, text_len);
        }
    }
    board.AddVia(via);
}

void PcbLoader::ParseTrace(const char* data, Board& board)
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

    int layer_id = static_cast<int>(ReadLE<uint32_t>(data));
    double x1 = static_cast<double>(ReadLE<int32_t>(data + 4)) / xyscale;
    double y1 = static_cast<double>(ReadLE<int32_t>(data + 8)) / xyscale;
    double x2 = static_cast<double>(ReadLE<int32_t>(data + 12)) / xyscale;
    double y2 = static_cast<double>(ReadLE<int32_t>(data + 16)) / xyscale;
    double width = static_cast<double>(ReadLE<int32_t>(data + 20)) / xyscale;  // Assuming 'scale' is width
    int net_id = static_cast<int>(ReadLE<uint32_t>(data + 24));

    Trace trace(layer_id, Vec2(x1, y1), Vec2(x2, y2), width);
    trace.SetNetId(net_id);  // Changed from trace.net_id
    board.AddTrace(trace);
}

void PcbLoader::ParseTextLabel(const char* data, Board& board, bool isStandalone)
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
    // label.layer = ReadLE<uint32_t>(&fileData[currentOffset]); -> data + 0
    // label.x = ReadLE<int32_t>(&fileData[currentOffset + 4]); -> data + 4
    // label.y = ReadLE<int32_t>(&fileData[currentOffset + 8]); -> data + 8
    // label.fontSize = ReadLE<uint32_t>(&fileData[currentOffset + 12]); -> data + 12 (text_size)
    // textLen = ReadLE<uint32_t>(&fileData[currentOffset + 24]); -> data + 24 (text_length)
    // text_start = data + 28
    // The `scale` in old TextLabel struct came from part_sub_type_06 `font_scale` or type_06 `divider`.
    // Visibility and ps06flag are only in part_sub_type_06.

    int layer_id = static_cast<int>(ReadLE<uint32_t>(data));                 // unknown_1 in hexpat, but layer in old loader
    double x = static_cast<double>(ReadLE<uint32_t>(data + 4) / xyscale);    // pos_x
    double y = static_cast<double>(ReadLE<uint32_t>(data + 8) / xyscale);    // pos_y
    double font_size = static_cast<double>(ReadLE<uint32_t>(data + 12));     // text_size
    double scale_factor = static_cast<double>(ReadLE<uint32_t>(data + 16));  // divider in hexpat, was text_scale in TextLabel.hpp
    // data + 20 is 'empty' (4 bytes), data + 24 is 'one' (2 bytes) then text_length (4 bytes from old loader) - this is slightly different from hexpat
    // XZZPCBLoader textLen was at +24. Text started at +28.
    // Let's trust XZZPCBLoader reading logic: fontSize @+12, textLen @+24, text @+28
    // This implies there are 12 bytes between fontSize and textLen (16, 20, 22 - this doesn't fit types) or between fontSize and text.
    // Old TextLabel struct: layer, x, y, fontSize, scale, visibility, ps06flag, text.
    // Hexpat standalone type_06: unknown_1 (layer), pos_x, pos_y, text_size (fontSize), divider (scale?), empty, one, text_length, text.
    // For standalone, we use fields based on old loader interpretation of type_06 block.

    uint32_t text_len = ReadLE<uint32_t>(data + 24);
    std::string text_content = ReadCB2312String(data + 28, text_len);

    TextLabel label = {text_content, Vec2(x, y), layer_id, font_size, scale_factor};
    // label.text_content = text_content;
    // label.coords = lbl_coords;
    // label.layer_id, font_size;
    // label.scale = scale_factor;  // From 'divider' field, which likely was a scale. -> Now passed in constructor
    // Standalone text labels from type 0x06 don't have visibility/ps06_flag in the same way as component text.
    // These are set to defaults in TextLabel.hpp.

    if (isStandalone) {
        board.AddStandaloneTextLabel(label);
    } else {
        // This case should ideally not be hit if parseComponent calls a different variant or populates directly.
        // However, if called from component parsing, the component itself should add it.
        // For now, we'll assume if !isStandalone, it means it was parsed inside a component
        // and the component logic will handle adding it to itself. This function just creates it.
        // To actually support this, the function should return TextLabel or take a Component&.
        // Let's re-evaluate when doing parseComponent. For now, it adds to board if standalone.
    }
}

void PcbLoader::ParseComponent(const char* rawComponentData, uint32_t componentBlockSize, Board& board)
{
    // Component data (type 0x07) is typically DES-encrypted.
    // First, make a mutable copy of the component's block data to decrypt it.

    // Initialize placeholder variables for TextLabel font_family and rotation
    std::string font_family_str = "";  // Default to empty string
    double rotation_degrees = 0.0;     // Default to 0.0 degrees

    std::vector<char> componentData(rawComponentData, rawComponentData + componentBlockSize);
    DecryptComponentBlock(componentData);  // Decrypts in-place

    uint32_t localOffset = 0;

    // Read outer component structure based on .hexpat type_07
    // uint32_t t07_block_size = componentBlockSize; (already known)
    if (componentData.size() < localOffset + 4) {
        return;
    }
    uint32_t part_overall_size = ReadLE<uint32_t>(&componentData[localOffset]);  // Size of the actual part data within this block
    localOffset += 4;

    if (componentData.size() < localOffset + 4) {
        return;
    }
    localOffset += 4;  // Skip padding (4 bytes)

    if (componentData.size() < localOffset + 4) { /*part_x*/
        return;
    }
    double part_x = static_cast<double>(ReadLE<uint32_t>(&componentData[localOffset])) / xyscale;
    localOffset += 4;
    if (componentData.size() < localOffset + 4) { /*part_y*/
        return;
    }
    double part_y = static_cast<double>(ReadLE<uint32_t>(&componentData[localOffset])) / xyscale;
    localOffset += 4;
    if (componentData.size() < localOffset + 4) { /*scale/padding*/
        return;
    }
	double part_rotation = static_cast<double>(ReadLE<uint32_t>(&componentData[localOffset])/xyscale);
    localOffset += 4;                             // rotation
    if (componentData.size() < localOffset + 2) { /*flags*/
        return;
    }
    localOffset += 2;  // Skip flags

    uint32_t pad_size_len = 0;
    if (localOffset + 4 <= componentData.size() && localOffset + 4 <= part_overall_size) {  // Check against part_overall_size for logical end too
        pad_size_len = ReadLE<uint32_t>(&componentData[localOffset]);
    } else {
        // Potentially return, but let's see if sub-blocks can still be parsed if part_overall_size is small but sub-blocks exist
    }
    localOffset += 4;

    std::string comp_footprint_name_str;
    if (pad_size_len > 0) {
        if (localOffset + pad_size_len <= componentData.size() && localOffset + pad_size_len <= part_overall_size) {  // Check against part_overall_size
            comp_footprint_name_str = ReadCB2312String(&componentData[localOffset], pad_size_len);
        } else {
            // Don't return, allow parsing of sub-blocks to continue if possible
        }
    }
    localOffset += pad_size_len;

    Component comp(comp_footprint_name_str, "", part_x, part_y);
    comp.footprint_name = comp_footprint_name_str;
	comp.rotation = part_rotation;


    while (localOffset < part_overall_size && localOffset < componentData.size()) {
        if (componentData.size() < localOffset + 1) {
            break;
        }
        uint8_t subType = static_cast<uint8_t>(componentData[localOffset++]);

        if (subType == 0x00) {
            break;
        }
        if (localOffset + 4 > componentData.size() || localOffset + 4 > part_overall_size) {
            break;
        }

        uint32_t subBlockSize = ReadLE<uint32_t>(&componentData[localOffset]);
        localOffset += 4;

        if (localOffset + subBlockSize > componentData.size()) {
            break;
        }
        // This check might be more relevant: ensure current offset for sub-block data isn't past the declared overall size
        if (localOffset > part_overall_size) {
            break;
        }

        const char* subBlockDataStart = &componentData[localOffset];

        switch (subType) {
            case 0x05: {  // Line Segment (part_sub_type_05)
                if (subBlockSize < 24) {
                    break;
                }
                // Check if reading fields will go out of subBlockSize bounds
                if (24 > subBlockSize) {  // Should be caught by above, but explicit check before reads
                    break;
                }
                int seg_layer = static_cast<int>(ReadLE<uint32_t>(subBlockDataStart));
                double x1 = static_cast<double>(ReadLE<int32_t>(subBlockDataStart + 4)) / xyscale;
                double y1 = static_cast<double>(ReadLE<int32_t>(subBlockDataStart + 8)) / xyscale;
                double x2 = static_cast<double>(ReadLE<int32_t>(subBlockDataStart + 12)) / xyscale;
                double y2 = static_cast<double>(ReadLE<int32_t>(subBlockDataStart + 16)) / xyscale;
                double thickness = static_cast<double>(ReadLE<uint32_t>(subBlockDataStart + 20)) / xyscale;  // Scale

                LineSegment seg {{x1, y1}, {x2, y2}, thickness, seg_layer};
                comp.graphical_elements.push_back(seg);
                break;
            }
            case 0x06: {  // Text Label (part_sub_type_06)
                // Min size: layer(4)x(4)y(4)fontSize(4)fontScale(4)padding(4)visible(1)ps06flag(1)nameSizeField(4) = 30. Text itself is extra.
                if (subBlockSize < 30) {
                    break;
                }
                int lbl_layer = static_cast<int>(ReadLE<uint32_t>(subBlockDataStart + 0));
                double lbl_x = static_cast<double>(ReadLE<uint32_t>(subBlockDataStart + 4));
                double lbl_y = static_cast<double>(ReadLE<uint32_t>(subBlockDataStart + 8));
                double lbl_font_size = static_cast<double>(ReadLE<uint32_t>(subBlockDataStart + 12));
                double lbl_font_scale = static_cast<double>(ReadLE<uint32_t>(subBlockDataStart + 16));
                // offset +20 is 4 bytes padding
                bool visible = (static_cast<uint8_t>(subBlockDataStart[24]) == 0x02);
                int ps06_flag = static_cast<uint8_t>(subBlockDataStart[25]);

                uint32_t nameSize = 0;
                if (subBlockSize >= 30) {  // Enough for nameSize field itself
                    nameSize = ReadLE<uint32_t>(subBlockDataStart + 26);
                } else {
                    break;
                }

                std::string lbl_text;
                if (nameSize > 0 && 30 + nameSize <= subBlockSize) {  // Check if text fits in subBlock
                    lbl_text = ReadCB2312String(subBlockDataStart + 30, nameSize);
                } else if (nameSize > 0) { /* Declared nameSize but not enough space */
                    break;
                }

                // TextLabel label(lbl_text, lbl_x, lbl_y, lbl_layer, lbl_font_size); // OLD
                auto label_ptr = std::make_unique<TextLabel>(lbl_text, Vec2(lbl_x, lbl_y), lbl_layer, lbl_font_size, lbl_font_scale);
                // label_ptr->scale = lbl_font_scale; // Now passed in constructor
                label_ptr->SetVisible(visible);
                label_ptr->font_family = font_family_str;  // Make sure to set all relevant fields
                label_ptr->rotation = rotation_degrees;    // Make sure to set all relevant fields
                // label.ps06_flag = ps06_flag; // ps06_flag is removed
                comp.text_labels.push_back(std::move(label_ptr));

                // Often, the first text label is the reference designator, second is value.
                if (comp.text_labels.size() == 1)
                    comp.reference_designator = lbl_text;
                if (comp.text_labels.size() == 2)
                    comp.value = lbl_text;

                break;
            }
            case 0x09: {  // Pin (part_sub_type_09)
                if (subBlockSize < 20 + 4) {
                    break;
                }

                uint32_t pin_offset_iterator = 0;  // Renamed from pin_ptr to avoid conflict
                // Initial padding
                if (pin_offset_iterator + 4 > subBlockSize) {
                    break;
                }
                pin_offset_iterator += 4;

                // Pin X
                if (pin_offset_iterator + 4 > subBlockSize) {
                    break;
                }
                double pin_x = static_cast<double>(ReadLE<int32_t>(subBlockDataStart + pin_offset_iterator)) / xyscale;
                pin_offset_iterator += 4;

                // Pin Y
                if (pin_offset_iterator + 4 > subBlockSize) {
                    break;
                }
                double pin_y = static_cast<double>(ReadLE<int32_t>(subBlockDataStart + pin_offset_iterator)) / xyscale;
                pin_offset_iterator += 4;
                // std::cout << "[PcbLoader LOG]         pin_y: " << pin_y << ", pin_offset_iterator: " << pin_offset_iterator << std::endl;

                // One 4-byte padding fields
                if (pin_offset_iterator + 4 > subBlockSize) {
                    break;
                }
                pin_offset_iterator += 4;
                // std::cout << "[PcbLoader LOG]         pin_offset_iterator after 4b padding: " << pin_offset_iterator << std::endl;
// Pin Rotation
                if (pin_offset_iterator + 4 > subBlockSize) {
                    break;
                }
                double pin_rotation = static_cast<double>(ReadLE<uint32_t>(subBlockDataStart + pin_offset_iterator)) / xyscale;
                pin_offset_iterator += 4;
                // Pin name size
                if (pin_offset_iterator + 4 > subBlockSize) {
                    break;
                }
                uint32_t pin_name_size = ReadLE<uint32_t>(subBlockDataStart + pin_offset_iterator);
                pin_offset_iterator += 4;

                std::string pin_name_str;  // Renamed from pin_name to avoid conflict with Pin member
                if (pin_name_size > 0) {
                    if (pin_offset_iterator + pin_name_size <= subBlockSize) {  // Check against subBlockSize
                        pin_name_str = ReadCB2312String(subBlockDataStart + pin_offset_iterator, pin_name_size);
                    } else {
                        // pin_name_str remains empty
                    }
                } else {
                }
                pin_offset_iterator += pin_name_size;

                // Create Pin with a default shape initially. It will be overwritten by parsed outline.
                CirclePad default_circle_pad_shape_obj;     // x_offset and y_offset default to 0.0
                default_circle_pad_shape_obj.radius = 0.1;  // Set the radius
                PadShape default_pad_shape = default_circle_pad_shape_obj;
                auto current_pin_object_ptr = std::make_unique<Pin>(Vec2(pin_x, pin_y), pin_name_str, default_pad_shape, Board::kBottomPinsLayer);  // Renamed pin_ptr to current_pin_object_ptr

                bool first_outline_processed = false;
                for (int i = 0; i < 4; ++i) {
                    if (pin_offset_iterator + 5 > subBlockSize) {
                        break;
                    }

                    bool isEndMarker = true;
                    for (int k = 0; k < 5; ++k) {
                        if (static_cast<uint8_t>(subBlockDataStart[pin_offset_iterator + k]) != 0) {
                            isEndMarker = false;
                            break;
                        }
                    }

                    if (isEndMarker) {
                        pin_offset_iterator += 5;
                        break;
                    }

                    if (pin_offset_iterator + 9 > subBlockSize) {
                        break;
                    }

                    // Read raw outline data
                    double outline_width_raw = static_cast<double>(ReadLE<uint32_t>(subBlockDataStart + pin_offset_iterator)) / xyscale;
                    pin_offset_iterator += 4;
                    double outline_height_raw = static_cast<double>(ReadLE<uint32_t>(subBlockDataStart + pin_offset_iterator)) / xyscale;
                    pin_offset_iterator += 4;
                    uint8_t outline_type_raw = static_cast<uint8_t>(subBlockDataStart[pin_offset_iterator++]);

                    if (!first_outline_processed) {                                                       // Only use the first outline to define the primary PadShape
                        if (outline_type_raw == 0x01) {                                                   // Circular or Capsule
                            if (outline_width_raw == outline_height_raw) {                                // Circle
                                current_pin_object_ptr->pad_shape = CirclePad {outline_width_raw / 2.0};  // Assuming radius from width/2
                            } else {                                                                      // Capsule
                                // For CapsulePad, width is total width, height is diameter/rect height
                                current_pin_object_ptr->pad_shape = CapsulePad {outline_width_raw, outline_height_raw};
                            }
                        } else if (outline_type_raw == 0x02) {  // Rectangular or Square
                            current_pin_object_ptr->pad_shape = RectanglePad {outline_width_raw, outline_height_raw};
                        } else {
                            // pin.pad_shape remains the default CirclePad{0.1}
                        }
                        first_outline_processed = true;
                    }
                    // The Pin class in Pin.hpp does not have a std::vector<PinOutline> outlines;
                    // nor does it have direct width/height members.
                    // If all outlines need to be stored, Pin.hpp would need modification.
                    // For now, we just use the first outline to define the pad_shape.
                }

                // Net ID is 12 bytes from the end of the pin sub-block.
                // (net_index 4 bytes + final padding 8 bytes = 12 bytes total for footer)
                uint32_t net_id_footer_size = 12;
                if (subBlockSize >= net_id_footer_size && subBlockSize >= pin_offset_iterator) {  // Ensure subBlock is large enough for footer AND pin_offset_iterator hasn't gone past subBlockSize
                    // Check if current pin_offset_iterator position allows for a 12-byte read if net_id was immediately next, OR if it's at a fixed end.
                    // The original logic: subBlockSize >= pin_offset_iterator + 12 (meaning 12 bytes *after* current ptr)
                    // AND subBlockSize >= 12 (absolute check)
                    // Then read from subBlockDataStart + subBlockSize - 12.
                    // This implies net_id is always at a fixed position from the end.
                    if (subBlockSize >= net_id_footer_size)  // ps08_footer_size is 8 bytes (u32 type + u32 value)
                    {
                        current_pin_object_ptr->SetNetId(static_cast<int>(ReadLE<uint32_t>(subBlockDataStart + subBlockSize - net_id_footer_size)));
                    } else {
                        current_pin_object_ptr->SetNetId(-1);  // Default if footer is too small
                    }
                } else {
                    current_pin_object_ptr->SetNetId(-1);  // Default if no valid type found
                }

                // Diode readings association
                if (diode_readings_type_ == 1 && !comp.reference_designator.empty() && !current_pin_object_ptr->pin_name.empty()) {
                    auto comp_it = diode_readings_.find(comp.reference_designator);
                    if (comp_it != diode_readings_.end()) {
                        auto pin_it = comp_it->second.find(current_pin_object_ptr->pin_name);
                        if (pin_it != comp_it->second.end()) {
                            current_pin_object_ptr->diode_reading = pin_it->second;
                        }
                    }
                } else if (diode_readings_type_ == 2 && current_pin_object_ptr->GetNetId() != -1) {
                    // This requires nets to be parsed first or a two-pass approach for diode readings.
                    // For now, this part of diode reading might not work perfectly if nets aren't ready.
                    // A placeholder for net name lookup:
                    // const Net* net = board.getNetById(pin.net_id);
                    // if (net && diodeReadings_.count(net->name) && diodeReadings_.at(net->name).count("0")) {
                    //    pin.diode_reading = diodeReadings_.at(net->name).at("0");
                    // }
                }

                // Update initial_width, initial_height, long_side, short_side from the final pad_shape
                // This also needs to use current_pin_object_ptr:
                std::tie(current_pin_object_ptr->width, current_pin_object_ptr->height) = Pin::GetDimensionsFromShape(current_pin_object_ptr->pad_shape);
                current_pin_object_ptr->long_side = std::max(current_pin_object_ptr->width, current_pin_object_ptr->height);
                current_pin_object_ptr->short_side = std::min(current_pin_object_ptr->width, current_pin_object_ptr->height);
                current_pin_object_ptr->coords.x_ax = pin_x;
                current_pin_object_ptr->coords.y_ax = pin_y;
                current_pin_object_ptr->rotation = pin_rotation;  // Assign the rotation data from file
                comp.pins.push_back(std::move(current_pin_object_ptr));  // Changed pin_ptr to current_pin_object_ptr
                break;
            }
            default:  // Unknown sub-type
                break;
        }
        localOffset += subBlockSize;  // Advance by the size of the sub-block we just processed
    }
    if (localOffset > part_overall_size && part_overall_size > 0) {
    }

    // If component reference designator is still not set (e.g. from type 0x06 text label), try to use footprint name or make a generic one.
    if (comp.reference_designator.empty()) {
        if (!comp.footprint_name.empty()) {
            comp.reference_designator = comp.footprint_name + "?";  // e.g. "0805?"
        } else {
            static int unnamed_comp_counter = 0;
            std::ostringstream ss;
            ss << "COMP?" << unnamed_comp_counter++;
            comp.reference_designator = ss.str();
        }
    }
    // DO STUFF AFTER PARSING BEFORE ADDING TO BOARD
    // Calculate component width and height and rotation
    // We need to ensure we actually calculate the center of the component, by getting the average of themin and max x and y coordinates of the component.
    // We will need to iterate through the graphical elements and to do this.

    if (!comp.graphical_elements.empty()) {
        double min_coord_x = std::numeric_limits<double>::max();
        double max_coord_x = std::numeric_limits<double>::lowest();
        double min_coord_y = std::numeric_limits<double>::max();
        double max_coord_y = std::numeric_limits<double>::lowest();

        for (const auto& seg : comp.graphical_elements) {
            min_coord_x = std::min({min_coord_x, seg.start.x_ax, seg.end.x_ax});
            max_coord_x = std::max({max_coord_x, seg.start.x_ax, seg.end.x_ax});
            min_coord_y = std::min({min_coord_y, seg.start.y_ax, seg.end.y_ax});
            max_coord_y = std::max({max_coord_y, seg.start.y_ax, seg.end.y_ax});
        }

        // Check if any coordinates were actually processed (i.e., bounds are not still at initial extreme values)
        if (max_coord_x >= min_coord_x && max_coord_y >= min_coord_y) {
            comp.width = max_coord_x - min_coord_x;
            comp.height = max_coord_y - min_coord_y;
            // Get center of the component
            comp.center_x = (min_coord_x + max_coord_x) / 2.0;
            comp.center_y = (min_coord_y + max_coord_y) / 2.0;
        }
        // If comp.width/height were pre-filled from footprint data and graphical_elements are very small or missing,
        // you might choose to keep the pre-filled ones or decide on a priority.
        // For now, this will overwrite them if graphical_elements exist and form a valid bbox.
    }
    // If graphical_elements is empty, comp.width and comp.height retain any values they had from earlier parsing stages.
    // ARBITRARY: Set all layers to one layer for now.
    comp.layer = Board::kTopCompLayer;  // TODO: Will need to handle top and bottom sides once we have the board 'folded'.
    board.AddComponent(comp);
}

bool PcbLoader::ParsePostV6Block(const std::vector<char>& fileData, Board& board, std::vector<char>::const_iterator v6_iter)
{
    // Logic adapted from XZZPCBLoader::parsePostV6Block
    // v6_iter points to the start of the "v6v6555v6v6" marker.
    // The actual data starts after this marker (11 bytes) and potentially some more skips.

    uint32_t currentFileOffset = static_cast<uint32_t>(std::distance(fileData.cbegin(), v6_iter)) + 11;  // Skip the marker itself

    if (currentFileOffset >= fileData.size())
        return false;

    // Original logic had a fixed skip of 7 bytes, then checked fileData[currentFileOffset]
    // Let's see if we can determine separator more dynamically based on the .hexpat approach for post_v6
    // The .hexpat's find_seperator() and related logic is complex to replicate directly here without its full context.
    // The old XZZPCBLoader had direct checks for 0x0A or 0x0D after skipping 7 bytes.

    currentFileOffset += 7;  // Skip 7 bytes as in original loader
    if (currentFileOffset >= fileData.size())
        return false;

    try {
        if (static_cast<uint8_t>(fileData[currentFileOffset]) == 0x0A) {
            // Type 1 diode readings (from old loader comments)
            // Format: 0x0A '=VOLTAGE=PART_NAME(PIN_NAME)'
            diode_readings_type_ = 1;
            while (currentFileOffset < fileData.size()) {
                currentFileOffset += 1;  // Skip 0x0A
                if (currentFileOffset >= fileData.size() || static_cast<uint8_t>(fileData[currentFileOffset]) != '=')
                    break;
                currentFileOffset += 1;  // Skip '='

                std::string voltageReading, partName, pinName;
                uint32_t startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != '=')
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break;
                voltageReading = ReadCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++;  // Skip '='

                startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != '(')
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break;
                partName = ReadCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++;  // Skip '('

                startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != ')')
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break;
                pinName = ReadCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++;  // Skip ')'

                diode_readings_[partName][pinName] = voltageReading;
            }
        } else {
            // Type 2 or Type 3 diode readings (from old loader comments)
            // Type 2 started with CD BC 0D 0A, then 'NET_NAME=VALUE'
            // Type 3 started with 0D 0A, then 'NET_NAME=VALUE'
            // The old code checked `fileData[currentPointer] != 0x0D` before the +=2 for Type 2, implying Type 2 might have an extra initial skip.
            if (static_cast<uint8_t>(fileData[currentFileOffset]) != 0x0D) {
                currentFileOffset += 2;  // Skip two unknown bytes for Type 2 (CD BC)
            }
            diode_readings_type_ = 2;

            while (currentFileOffset < fileData.size()) {
                if (currentFileOffset + 2 > fileData.size())
                    break;
                if (static_cast<uint8_t>(fileData[currentFileOffset]) != 0x0D || static_cast<uint8_t>(fileData[currentFileOffset + 1]) != 0x0A)
                    break;               // Expect 0D 0A separator
                currentFileOffset += 2;  // Skip 0x0D 0x0A

                if (currentFileOffset >= fileData.size())
                    break;
                // Check for double 0D0A which might signify end of this section
                if (currentFileOffset + 2 <= fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) == 0x0D && static_cast<uint8_t>(fileData[currentFileOffset + 1]) == 0x0A) {
                    break;
                }

                std::string netName, value;
                uint32_t startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != '=')
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break;
                netName = ReadCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++;  // Skip '='

                startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != 0x0D)
                    currentFileOffset++;
                if (currentFileOffset >= fileData.size())
                    break;  // Value should end before 0D (start of next 0D 0A)
                value = ReadCB2312String(&fileData[startPos], currentFileOffset - startPos);

                diode_readings_[netName]["0"] = value;  // Use "0" as pin key for this type
            }
        }
        return true;
    } catch (const std::runtime_error& e) {
        // Log e.what()
        return false;
    }
}

bool PcbLoader::ParseNetBlock(const std::vector<char>& fileData, Board& board, uint32_t netDataOffset)
{
    if (netDataOffset == 0 || netDataOffset >= fileData.size()) {
        return true;  // No net block or invalid offset, considered okay.
    }

    try {
        // Net block starts with its total size (uint32_t)
        if (netDataOffset + 4 > fileData.size()) {
            // Not enough data even for the block size itself
            return false;
        }
        uint32_t netBlockTotalSize = ReadLE<uint32_t>(&fileData[netDataOffset]);

        uint32_t currentRelativeOffset = 4;                  // Start reading after the netBlockTotalSize field
        uint32_t netBlockEndOffset = 4 + netBlockTotalSize;  // Relative end offset within the conceptual net block data

        while (currentRelativeOffset < netBlockEndOffset) {
            // Each net entry: net_record_size (u32), net_id (u32), net_name (char[net_record_size - 8])
            if (netDataOffset + currentRelativeOffset + 8 > fileData.size()) {
                // Not enough data for net_record_size and net_id fields
                // This indicates a malformed block or premature end.
                return false;
            }

            uint32_t netRecordSize = ReadLE<uint32_t>(&fileData[netDataOffset + currentRelativeOffset]);
            uint32_t netId = ReadLE<uint32_t>(&fileData[netDataOffset + currentRelativeOffset + 4]);

            if (netRecordSize < 8) {
                // Record size must be at least 8 to hold its own size and the ID.
                // If smaller, it's corrupt. Advance past this problematic record as declared.
                // Log_Warning("Invalid net record size %u at offset %u, skipping", netRecordSize, netDataOffset + currentRelativeOffset);
                if (netRecordSize == 0)
                    return false;  // Avoid infinite loop if netRecordSize is 0
                currentRelativeOffset += netRecordSize;
                continue;
            }

            uint32_t netNameLength = netRecordSize - 8;
            if (netDataOffset + currentRelativeOffset + 8 + netNameLength > fileData.size() || 8 + netNameLength > netRecordSize) {
                // Net name would extend beyond file data, or netRecordSize is inconsistent.
                // Log_Error("Net record for ID %u at offset %u claims size %u but data is insufficient or inconsistent.", netId, netDataOffset + currentRelativeOffset, netRecordSize);
                return false;
            }

            std::string netName;
            if (netNameLength > 0) {
                netName = ReadCB2312String(&fileData[netDataOffset + currentRelativeOffset + 8], netNameLength);
            }

            // Use emplace to construct Net in place, avoiding issues with default constructor
            board.AddNet(Net(static_cast<int>(netId), netName));

            currentRelativeOffset += netRecordSize;
        }

        // Check if we consumed exactly the amount of data specified by netBlockTotalSize
        if (currentRelativeOffset != netBlockEndOffset) {
            // Log_Warning("Net block parsing finished, but read %u bytes, expected %u based on netBlockTotalSize.", currentRelativeOffset - 4, netBlockTotalSize);
            // This might indicate an issue with the last record or the total size field.
        }

        return true;
    } catch (const std::runtime_error& e) {
        // Log_Exception(e, "Exception during net block parsing.");
        return false;
    }
}

void PcbLoader::ApplyGlobalCoordinateMirroring(Board& board)
{
    // Apply a static global X-coordinate mirror to correct coordinate system mismatch
    // between board files and physical layout. This is separate from interactive transformations.

    // Get board bounds to determine the center axis for mirroring
    BLRect board_bounds = board.GetBoundingBox(false);
    if (board_bounds.w <= 0 && board_bounds.h <= 0) {
        // No valid bounds, skip mirroring
        return;
    }

    double center_x = board_bounds.x + board_bounds.w / 2.0;

    // Mirror all elements across the center axis
    for (auto& layer_pair : board.m_elements_by_layer) {
        for (auto& element_ptr : layer_pair.second) {
            if (!element_ptr) continue;

            // Apply element-specific mirroring
            if (auto trace = dynamic_cast<Trace*>(element_ptr.get())) {
                // Mirror trace endpoints
                trace->x1 = 2 * center_x - trace->x1;
                trace->x2 = 2 * center_x - trace->x2;
            }
            else if (auto arc = dynamic_cast<Arc*>(element_ptr.get())) {
                // Mirror arc center and adjust angles
                arc->center.x_ax = 2 * center_x - arc->center.x_ax;

                // For horizontal mirroring, transform angles and swap start/end
                // Original arc from start_angle to end_angle becomes
                // arc from (180° - end_angle) to (180° - start_angle)
                double original_start = arc->start_angle;
                double original_end = arc->end_angle;

                arc->start_angle = 180.0 - original_end;
                arc->end_angle = 180.0 - original_start;

                // Normalize angles to [0, 360)
                while (arc->start_angle < 0) arc->start_angle += 360.0;
                while (arc->end_angle < 0) arc->end_angle += 360.0;
                while (arc->start_angle >= 360.0) arc->start_angle -= 360.0;
                while (arc->end_angle >= 360.0) arc->end_angle -= 360.0;
            }
            else if (auto via = dynamic_cast<Via*>(element_ptr.get())) {
                // Mirror via position
                via->x = 2 * center_x - via->x;
            }
            else if (auto text = dynamic_cast<TextLabel*>(element_ptr.get())) {
                // Mirror text position
                text->coords.x_ax = 2 * center_x - text->coords.x_ax;
            }
            else if (auto comp = dynamic_cast<Component*>(element_ptr.get())) {
                // Mirror component using its Mirror method
                comp->Mirror(center_x);
            }
        }
    }

    std::cout << "Applied global coordinate mirroring around center axis X=" << center_x << std::endl;
}

// Implementations for parsePostV6Block, and element-specific parsers (parseArc, parseVia, etc.) will follow.
// ... (make sure this is placed correctly relative to other definitions) ...

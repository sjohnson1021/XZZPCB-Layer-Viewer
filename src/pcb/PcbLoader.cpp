#include "PcbLoader.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm> // For std::search
#include <cstring>   // For std::memcpy
#include <iomanip>   // For std::setw, std::setfill, std::hex
#include <sstream>   // For std::ostringstream
#include <limits>    // For std::numeric_limits

// Static data for DES key generation, similar to old XZZPCBLoader.cpp
static const unsigned char hexconv[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0,  0,  0,  0,  0,  10, 11, 12, 13, 14, 15, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0,  0,  0,  0,  0,  0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};
static const std::vector<uint16_t> des_key_byte_list = {0xE0, 0xCF, 0x2E, 0x9F, 0x3C, 0x33, 0x3C, 0x33};

std::unique_ptr<Board> PcbLoader::loadFromFile(const std::string& filePath) {
    std::vector<char> fileData;
    if (!readFileData(filePath, fileData)) {
        // Consider logging an error here
        return nullptr;
    }

    if (!verifyFormat(fileData)) {
        // Consider logging an error here
        return nullptr;
    }

    if (!decryptFileDataIfNeeded(fileData)) {
        // Consider logging an error here
        return nullptr;
    }

    auto board = std::make_unique<Board>();
    board->file_path = filePath;
    // board->board_name = extract from file path or header?

    uint32_t mainDataOffset = 0;
    uint32_t netDataOffset = 0;
    uint32_t imageDataOffset = 0; // Currently unused by parser logic beyond header
    uint32_t mainDataBlocksSize = 0;

    if (!parseHeader(fileData, *board, mainDataOffset, netDataOffset, imageDataOffset, mainDataBlocksSize)) {
        return nullptr; // Error parsing header
    }

    if (!parseMainDataBlocks(fileData, *board, mainDataOffset, mainDataBlocksSize)) {
        return nullptr; // Error parsing main data blocks
    }

    if (!parseNetBlock(fileData, *board, netDataOffset)) {
        return nullptr; // Error parsing net block
    }
    
    // Handle post-v6 data if present (diode readings)
    static const std::vector<uint8_t> v6_marker = {0x76, 0x36, 0x76, 0x36, 0x35, 0x35, 0x35, 0x76, 0x36, 0x76, 0x36};
    auto v6_iter = std::search(fileData.cbegin(), fileData.cend(), v6_marker.cbegin(), v6_marker.cend());
    if (v6_iter != fileData.cend()) {
        if (!parsePostV6Block(fileData, *board, v6_iter)) {
             // Optional: log error but continue, as this might be auxiliary data
        }
    }

    return board;
}

bool PcbLoader::readFileData(const std::string& filePath, std::vector<char>& fileData) {
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

bool PcbLoader::verifyFormat(const std::vector<char>& fileData) {
    if (fileData.size() < 6) return false;
    const char* signature = "XZZPCB";
    if (std::memcmp(fileData.data(), signature, 6) == 0) return true;

    // Check for XORed encrypted format
    if (fileData.size() >= 0x11) { // Need at least 0x10 + 1 byte
        uint8_t xor_key = static_cast<uint8_t>(fileData[0x10]);
        if (xor_key != 0x00) { // If 0x00, it's not XORed this way or already decrypted
            bool match = true;
            for (int i = 0; i < 6; ++i) {
                if ((static_cast<uint8_t>(fileData[i]) ^ xor_key) != static_cast<uint8_t>(signature[i])) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

// Handles the initial XOR decryption of the entire file if needed.
// The DES decryption for component blocks is handled separately in parseComponent.
bool PcbLoader::decryptFileDataIfNeeded(std::vector<char>& fileData) {
    if (fileData.size() < 0x11 || static_cast<uint8_t>(fileData[0x10]) == 0x00) {
        return true; // Not encrypted this way or too small
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
void PcbLoader::decryptComponentBlock(std::vector<char>& blockData) {
    std::ostringstream key_hex_stream;
    key_hex_stream.str().reserve(16); // DES key is 8 bytes = 16 hex chars

    for (size_t i = 0; i < des_key_byte_list.size(); i += 2) {
        uint16_t value = (des_key_byte_list[i] << 8) | des_key_byte_list[i+1];
        value ^= 0x3C33; // Specific XOR for this key derivation
        key_hex_stream << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << value;
    }
    std::string key_hex_str = key_hex_stream.str();

    uint64_t des_key = 0;
    for (int i = 0; i < 8; ++i) {
        unsigned char byte_str[3] = {static_cast<unsigned char>(key_hex_str[i*2]), static_cast<unsigned char>(key_hex_str[i*2+1]), '\0'};
        uint64_t byte_val = hexconv[byte_str[0]] * 16 + hexconv[byte_str[1]];
        des_key |= (byte_val << ((7 - i) * 8));
    }

    std::vector<char> decrypted_data;
    decrypted_data.reserve(blockData.size());

    const char* p = blockData.data();
    const char* ep = p + blockData.size();

    while (p < ep) {
        if (std::distance(p, ep) < 8) break; // Not enough data for a full DES block

        uint64_t encrypted_block64 = 0;
        // Read block as big-endian for DES function
        for(int i=0; i<8; ++i) {
            encrypted_block64 = (encrypted_block64 << 8) | static_cast<uint8_t>(p[i]);
        }
        
        uint64_t decrypted_block64 = des(encrypted_block64, des_key, 'd');

        // Write decrypted block back (DES output is big-endian)
        for (int i = 7; i >= 0; --i) {
            decrypted_data.push_back(static_cast<char>((decrypted_block64 >> (i * 8)) & 0xFF));
        }
        p += 8;
    }
    blockData = decrypted_data; // Replace original with decrypted version
}

std::string PcbLoader::readCB2312String(const char* data, size_t length) {
    std::string result;
    result.reserve(length); // Approximate reservation
    bool last_was_high_byte = false;

    for (size_t i = 0; i < length; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < 0x80) { // ASCII
            result += static_cast<char>(c);
            last_was_high_byte = false;
        } else { // Potentially part of a multi-byte character (e.g., GB2312)
            // For now, treat non-ASCII as '?'
            // A proper GB2312 to UTF-8 conversion would be more robust.
            if (!last_was_high_byte) { // First byte of a potential 2-byte char
                result += '?';
            }
            last_was_high_byte = !last_was_high_byte;
        }
    }
    return result;
}

bool PcbLoader::parseHeader(const std::vector<char>& fileData, Board& board,
                              uint32_t& outMainDataOffset, uint32_t& outNetDataOffset,
                              uint32_t& outImageDataOffset, uint32_t& outMainDataBlocksSize) {
    if (fileData.size() < 0x44) { // Minimum size for the header fields we need
        return false; // Error: File too small for header
    }

    try {
        // According to XZZPCB Decrypted.hexpat and old XZZPCBLoader.cpp:
        // - image_block_start is at 0x24, relative to 0x20
        // - net_block_start is at 0x28, relative to 0x20
        // - main_data_blocks_size is at 0x40 (absolute address in fileData)

        // uint32_t header_addresses_size = readLE<uint32_t>(&fileData[0x20]); // This was in old loader, seems unused for critical offsets by old loader.
        
        // Aligning with XZZPCBLoader.cpp: offsets are added to 0x20 unconditionally
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
            (outImageDataOffset > 0 && outImageDataOffset > fileData.size())) {
            // Potentially corrupted offsets or file too small for declared sizes
            // However, some files might have 0 for net/image offsets if blocks are absent.
            // The main block size is most critical here.
            if (outMainDataOffset + 4 + outMainDataBlocksSize > fileData.size() && outMainDataBlocksSize > 0) {
                 return false; // Main data block seems to exceed file bounds
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

bool PcbLoader::parseMainDataBlocks(const std::vector<char>& fileData, Board& board,
                                    uint32_t mainDataOffset, uint32_t mainDataBlocksSize) {
    if (mainDataBlocksSize == 0) return true; // No main data to parse

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
            if (readLE<uint32_t>(&fileData[currentOffset]) == 0) {
                currentOffset += 4;
                continue;
            }

            uint8_t blockType = static_cast<uint8_t>(fileData[currentOffset]);
            currentOffset += 1;
            
            uint32_t blockSize = readLE<uint32_t>(&fileData[currentOffset]);
            currentOffset += 4;

            // Boundary check for the block content itself
            if (currentOffset + blockSize > fileData.size() || currentOffset + blockSize > endOffsetOfDataRegion) {
                // Block claims to extend beyond file data or its allocated region
                return false; // Critical error
            }

            const char* blockDataStart = &fileData[currentOffset];

            switch (blockType) {
                case 0x01: // Arc
                    parseArc(blockDataStart, board);
                    break;
                case 0x02: // Via
                    parseVia(blockDataStart, board);
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
                    parseVia(blockDataStart, board); // TEMPORARY: treat as via. TODO: Revisit this.
                    break;
                default:
                    // Unknown or unhandled block type
                    // std::cout << "Skipping unknown block type 0x" << std::hex << (int)blockType << std::dec << " of size " << blockSize << std::endl;
                    break;
            }
            currentOffset += blockSize;
        }
        return true;
    } catch (const std::runtime_error& e) { // Catch potential errors from readLE
        // Log e.what()
        return false;
    }
}

void PcbLoader::parseArc(const char* data, Board& board) {
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
    double cx = static_cast<double>(readLE<uint32_t>(data + 4));
    double cy = static_cast<double>(readLE<uint32_t>(data + 8));
    double radius = static_cast<double>(readLE<int32_t>(data + 12));
    double start_angle = static_cast<double>(readLE<int32_t>(data + 16));
    double end_angle = static_cast<double>(readLE<int32_t>(data + 20));
    double thickness = static_cast<double>(readLE<int32_t>(data + 24)); // Assuming 'scale' is thickness
    int net_id = static_cast<int>(readLE<int32_t>(data + 28));
    // int32_t unknown_arc_val = readLE<int32_t>(data + 32); // If needed

    Arc arc(layer_id, cx, cy, radius, start_angle, end_angle);
    arc.thickness = thickness;
    arc.net_id = net_id;
    board.addArc(arc);
}

void PcbLoader::parseVia(const char* data, Board& board) {
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

    double x = static_cast<double>(readLE<int32_t>(data));
    double y = static_cast<double>(readLE<int32_t>(data + 4));
    double radius_a = static_cast<double>(readLE<int32_t>(data + 8));  // Pad radius on layer_a
    double radius_b = static_cast<double>(readLE<int32_t>(data + 12)); // Pad radius on layer_b
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
    
    if (text_len > 0) {
        // Ensure not to read past block boundary, though blockSize check in parseMainDataBlocks helps.
        // The via_text_length in hexpat for type_02 says it's typically 0 or 1, meaning a single char.
        // If it's truly a length, then data + 32 is the start of text.
        // The old XZZPCBLoader has: via.text = std::string(&fileData[currentOffset + 32], textLen);
        // So, text_len is indeed the length of the string starting at offset 32 from blockDataStart.
        via.optional_text = readCB2312String(data + 32, text_len);
    }
    board.addVia(via);
}

void PcbLoader::parseTrace(const char* data, Board& board) {
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
    double x1 = static_cast<double>(readLE<int32_t>(data + 4));
    double y1 = static_cast<double>(readLE<int32_t>(data + 8));
    double x2 = static_cast<double>(readLE<int32_t>(data + 12));
    double y2 = static_cast<double>(readLE<int32_t>(data + 16));
    double width = static_cast<double>(readLE<int32_t>(data + 20)); // Assuming 'scale' is width
    int net_id = static_cast<int>(readLE<uint32_t>(data + 24));

    Trace trace(layer_id, x1, y1, x2, y2, width);
    trace.net_id = net_id;
    board.addTrace(trace);
}

void PcbLoader::parseTextLabel(const char* data, Board& board, bool isStandalone) {
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
    // Offsets from XZZPCBLoader.cpp for standalone text (type 0x06):
    // label.layer = readLE<uint32_t>(&fileData[currentOffset]); -> data + 0
    // label.x = readLE<int32_t>(&fileData[currentOffset + 4]); -> data + 4
    // label.y = readLE<int32_t>(&fileData[currentOffset + 8]); -> data + 8
    // label.fontSize = readLE<uint32_t>(&fileData[currentOffset + 12]); -> data + 12 (text_size)
    // textLen = readLE<uint32_t>(&fileData[currentOffset + 24]); -> data + 24 (text_length)
    // text_start = data + 28
    // The `scale` in old TextLabel struct came from part_sub_type_06 `font_scale` or type_06 `divider`.
    // Visibility and ps06flag are only in part_sub_type_06.

    int layer_id = static_cast<int>(readLE<uint32_t>(data)); // unknown_1 in hexpat, but layer in old loader
    double x = static_cast<double>(readLE<uint32_t>(data + 4)); // pos_x
    double y = static_cast<double>(readLE<uint32_t>(data + 8)); // pos_y
    double font_size = static_cast<double>(readLE<uint32_t>(data + 12)); // text_size
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

    if (isStandalone) {
        board.addStandaloneTextLabel(label);
    } else {
        // This case should ideally not be hit if parseComponent calls a different variant or populates directly.
        // However, if called from component parsing, the component itself should add it.
        // For now, we'll assume if !isStandalone, it means it was parsed inside a component
        // and the component logic will handle adding it to itself. This function just creates it.
        // To actually support this, the function should return TextLabel or take a Component&.
        // Let's re-evaluate when doing parseComponent. For now, it adds to board if standalone.
    }
}

void PcbLoader::parseComponent(const char* rawComponentData, uint32_t componentBlockSize, Board& board) {
    // Component data (type 0x07) is typically DES-encrypted.
    // First, make a mutable copy of the component's block data to decrypt it.
    std::vector<char> componentData(rawComponentData, rawComponentData + componentBlockSize);
    decryptComponentBlock(componentData); // Decrypts in-place

    uint32_t localOffset = 0;

    // Read outer component structure based on .hexpat type_07
    // uint32_t t07_block_size = componentBlockSize; (already known)
    uint32_t part_overall_size = readLE<uint32_t>(&componentData[localOffset]); // Size of the actual part data within this block
    localOffset += 4;
    localOffset += 4; // Skip padding (4 bytes)

    double part_x = static_cast<double>(readLE<uint32_t>(&componentData[localOffset]));
    localOffset += 4;
    double part_y = static_cast<double>(readLE<uint32_t>(&componentData[localOffset]));
    localOffset += 4;
    localOffset += 4; // Skip scale/padding (4 bytes)

    // uint8_t flag1 = componentData[localOffset++]; // Referenced in .hexpat, seems unused in old loader logic
    // uint8_t flag2 = componentData[localOffset++];
    localOffset += 2; // Skip flags

    uint32_t pad_size_len = 0; 
    if (localOffset + 4 <= componentData.size() && localOffset + 4 <= part_overall_size) {
        pad_size_len = readLE<uint32_t>(&componentData[localOffset]);
    } else {
        // Not enough data for pad_size_len, critical failure for this component.
        // Log_Error("Component parsing: Not enough data for pad_size_len.");
        return; // Stop parsing this component
    }
    localOffset += 4;

    std::string comp_footprint_name_str;
    if (pad_size_len > 0) {
        if (localOffset + pad_size_len <= componentData.size() && localOffset + pad_size_len <= part_overall_size) {
            comp_footprint_name_str = readCB2312String(&componentData[localOffset], pad_size_len);
        } else {
            // Length specified but not enough data, critical failure for this component.
            // Log_Error("Component parsing: Not enough data for footprint name string.");
            return; // Stop parsing this component
        }
    }
    localOffset += pad_size_len;

    Component comp(comp_footprint_name_str, "", part_x, part_y);
    comp.footprint_name = comp_footprint_name_str;

    while (localOffset < part_overall_size && localOffset < componentData.size()) {
        if (componentData.size() < localOffset + 1) break; 
        uint8_t subType = static_cast<uint8_t>(componentData[localOffset++]);

        if (subType == 0x00) break; 
        if (localOffset + 4 > componentData.size() || localOffset + 4 > part_overall_size) break; 
        
        uint32_t subBlockSize = readLE<uint32_t>(&componentData[localOffset]);
        localOffset += 4;

        if (localOffset + subBlockSize > componentData.size() || localOffset + subBlockSize > part_overall_size) {
            break; 
        }
        
        const char* subBlockDataStart = &componentData[localOffset];

        switch (subType) {
            case 0x05: { // Line Segment (part_sub_type_05)
                if (subBlockSize < 24) { /* Min size for a segment with layer, 2x points, scale */ break; }
                int seg_layer = static_cast<int>(readLE<uint32_t>(subBlockDataStart));
                double x1 = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 4));
                double y1 = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 8));
                double x2 = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 12));
                double y2 = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 16));
                double thickness = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 20)); // Scale
                // 4 bytes padding after scale in .hexpat
                LineSegment seg{{x1, y1}, {x2, y2}, thickness, seg_layer};
                comp.graphical_elements.push_back(seg);
                break;
            }
            case 0x06: { // Text Label (part_sub_type_06)
                // Min size: layer(4)x(4)y(4)fontSize(4)fontScale(4)padding(4)visible(1)ps06flag(1)nameSizeField(4) = 30. Text itself is extra.
                if (subBlockSize < 30) { break; }
                int lbl_layer = static_cast<int>(readLE<uint32_t>(subBlockDataStart + 0));
                double lbl_x = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 4));
                double lbl_y = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 8));
                double lbl_font_size = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 12));
                double lbl_font_scale = static_cast<double>(readLE<uint32_t>(subBlockDataStart + 16));
                // offset +20 is 4 bytes padding
                bool visible = (static_cast<uint8_t>(subBlockDataStart[24]) == 0x02);
                int ps06_flag = static_cast<uint8_t>(subBlockDataStart[25]);
                
                uint32_t nameSize = 0;
                if (subBlockSize >= 30) { // Enough for nameSize field itself
                    nameSize = readLE<uint32_t>(subBlockDataStart + 26);
                } else { break; }

                std::string lbl_text;
                if (nameSize > 0 && 30 + nameSize <= subBlockSize) { // Check if text fits in subBlock
                    lbl_text = readCB2312String(subBlockDataStart + 30, nameSize);
                } else if (nameSize > 0) { /* Declared nameSize but not enough space */ break; }

                TextLabel label(lbl_text, lbl_x, lbl_y, lbl_layer, lbl_font_size);
                label.scale = lbl_font_scale;
                label.is_visible = visible;
                label.ps06_flag = ps06_flag;
                comp.text_labels.push_back(label);

                // Often, the first text label is the reference designator, second is value.
                if (comp.text_labels.size() == 1) comp.reference_designator = lbl_text;
                if (comp.text_labels.size() == 2) comp.value = lbl_text;

                break;
            }
            case 0x09: { // Pin (part_sub_type_09)
                // Minimum size: padding(4) + xy(8) + padding(8) + name_size_field(4) = 24, plus name, plus outlines, plus net_id + padding (12)
                // A pin must at least have its name_size field, so check up to that point + name_size field (20 bytes for fields before name string)
                if (subBlockSize < 20 + 4) { /* Not even enough for pin_name_size field */ break; }
                uint32_t pin_ptr = 0;
                pin_ptr += 4; // Skip initial padding (offset 0)
                double pin_x = static_cast<double>(readLE<uint32_t>(subBlockDataStart + pin_ptr)); pin_ptr +=4; // offset 4
                double pin_y = static_cast<double>(readLE<uint32_t>(subBlockDataStart + pin_ptr)); pin_ptr +=4; // offset 8
                pin_ptr += 8; // Skip two 4-byte padding fields (offsets 12, 16)
                
                uint32_t pin_name_size = readLE<uint32_t>(subBlockDataStart + pin_ptr); // pin_ptr is now 20
                pin_ptr += 4; // pin_ptr is now 24
                
                std::string pin_name;
                if (pin_name_size > 0 && pin_ptr + pin_name_size <= subBlockSize) {
                    pin_name = readCB2312String(subBlockDataStart + pin_ptr, pin_name_size);
                } else if (pin_name_size > 0) { /* Declared pin_name_size but not enough space */ break; }
                pin_ptr += pin_name_size;

                Pin pin(pin_x, pin_y, pin_name, CirclePad{0.1}); // Default shape

                // Parse Pin Outlines
                for (int i=0; i<4; ++i) {
                    if (pin_ptr + 5 > subBlockSize) break; // Not enough for an outline end marker or next outline field
                    if (readLE<uint32_t>(subBlockDataStart + pin_ptr) == 0 && 
                        static_cast<uint8_t>(subBlockDataStart[pin_ptr + 4]) == 0) {
                        pin_ptr += 5; // Consume end marker
                        break;
                    }
                    if (pin_ptr + 9 > subBlockSize) break; // Not enough for full outline (width, height, type)
                    double o_width = static_cast<double>(readLE<uint32_t>(subBlockDataStart + pin_ptr)); pin_ptr += 4;
                    double o_height = static_cast<double>(readLE<uint32_t>(subBlockDataStart + pin_ptr)); pin_ptr += 4;
                    uint8_t o_type = static_cast<uint8_t>(subBlockDataStart[pin_ptr++]);

                    if (i == 0) { // Only process the first outline for the shape
                        if (o_type == 0x01) { // Circular
                            pin.pad_shape = CirclePad{0.0, 0.0, std::min(o_width, o_height) / 2.0}; // Assuming radius from min dim
                        } else if (o_type == 0x02) { // Rectangular
                            // Heuristic: if very thin, could be a line-like capsule, but let's stick to rect/square
                            pin.pad_shape = RectanglePad{0.0, 0.0, o_width, o_height};
                        } else { // Oblong/Capsule or other? For now, default to circle based on average.
                             // The old XZZPCBLoader logic used CapsulePad if width != height for type 0x01.
                             // And SquarePad if width == height for type 0x02.
                             // This suggests type 0x01 was more general round/oblong and 0x02 was rect/square.
                             // Re-evaluating old logic:
                             // if (isRounded == 1) { -> type 0x01
                             //    if (width == height) pin.geometry = CirclePad{width/2.0};
                             //    else pin.geometry = CapsulePad{height, width};
                             // } else { -> type 0x02
                             //    if (width == height) pin.geometry = SquarePad{width};
                             //    else pin.geometry = RectanglePad{height, width};
                             // }
                            if (o_type == 0x01) { // Circular or Capsule
                                if (o_width == o_height) pin.pad_shape = CirclePad{0.0,0.0,o_width/2.0};
                                else pin.pad_shape = CapsulePad{0.0,0.0,o_width, o_height}; // width, height for capsule
                            } else { // Rectangular or Square (assuming type 0x00 or 0x02)
                                 pin.pad_shape = RectanglePad{0.0,0.0,o_width, o_height};
                            }
                        }
                    }
                }
                
                // Net ID is 12 bytes from the end of the pin sub-block.
                // (net_index 4 bytes + final padding 8 bytes = 12 bytes total for footer)
                if (subBlockSize >= pin_ptr + 12 && subBlockSize >= 12) { // Ensure subBlock is large enough for footer AND current pin_ptr hasn't overrun
                     pin.net_id = static_cast<int>(readLE<uint32_t>(subBlockDataStart + subBlockSize - 12));
                } else {
                    pin.net_id = -1; // Default or indicate error/not present
                }
                // TODO: Diode readings for pins, similar to old XZZPCBLoader.cpp
                // This needs comp.reference_designator to be stable (usually from 1st label)
                // and net names from the main nets map.
                if (diodeReadingsType_ == 1 && !comp.reference_designator.empty()) {
                    if (diodeReadings_.count(comp.reference_designator) && diodeReadings_.at(comp.reference_designator).count(pin.pin_name)) {
                        pin.diode_reading = diodeReadings_.at(comp.reference_designator).at(pin.pin_name);
                    }
                } else if (diodeReadingsType_ == 2 && pin.net_id != -1) {
                    // This requires nets to be parsed first or a two-pass approach for diode readings.
                    // For now, this part of diode reading might not work perfectly if nets aren't ready.
                    // A placeholder for net name lookup:
                    // const Net* net = board.getNetById(pin.net_id);
                    // if (net && diodeReadings_.count(net->name) && diodeReadings_.at(net->name).count("0")) {
                    //    pin.diode_reading = diodeReadings_.at(net->name).at("0");
                    // }
                }

                comp.pins.push_back(pin);
                break;
            }
            default: // Unknown sub-type
                break;
        }
        localOffset += subBlockSize; // Advance by the size of the sub-block we just processed
    }
    board.addComponent(comp);
}

bool PcbLoader::parsePostV6Block(const std::vector<char>& fileData, Board& board,
                                 std::vector<char>::const_iterator v6_iter) {
    // Logic adapted from XZZPCBLoader::parsePostV6Block
    // v6_iter points to the start of the "v6v6555v6v6" marker.
    // The actual data starts after this marker (11 bytes) and potentially some more skips.

    uint32_t currentFileOffset = static_cast<uint32_t>(std::distance(fileData.cbegin(), v6_iter)) + 11; // Skip the marker itself

    if (currentFileOffset >= fileData.size()) return false;

    // Original logic had a fixed skip of 7 bytes, then checked fileData[currentFileOffset]
    // Let's see if we can determine separator more dynamically based on the .hexpat approach for post_v6
    // The .hexpat's find_seperator() and related logic is complex to replicate directly here without its full context.
    // The old XZZPCBLoader had direct checks for 0x0A or 0x0D after skipping 7 bytes.

    currentFileOffset += 7; // Skip 7 bytes as in original loader
    if (currentFileOffset >= fileData.size()) return false;

    try {
        if (static_cast<uint8_t>(fileData[currentFileOffset]) == 0x0A) {
            // Type 1 diode readings (from old loader comments)
            // Format: 0x0A '=VOLTAGE=PART_NAME(PIN_NAME)'
            diodeReadingsType_ = 1;
            while (currentFileOffset < fileData.size()) {
                currentFileOffset += 1; // Skip 0x0A
                if (currentFileOffset >= fileData.size() || static_cast<uint8_t>(fileData[currentFileOffset]) != '=') break;
                currentFileOffset += 1; // Skip '='

                std::string voltageReading, partName, pinName;
                uint32_t startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != '=') currentFileOffset++;
                if (currentFileOffset >= fileData.size()) break;
                voltageReading = readCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++; // Skip '='

                startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != '(') currentFileOffset++;
                if (currentFileOffset >= fileData.size()) break;
                partName = readCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++; // Skip '('

                startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != ')') currentFileOffset++;
                if (currentFileOffset >= fileData.size()) break;
                pinName = readCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++; // Skip ')'
                
                diodeReadings_[partName][pinName] = voltageReading;
            }
        } else {
            // Type 2 or Type 3 diode readings (from old loader comments)
            // Type 2 started with CD BC 0D 0A, then 'NET_NAME=VALUE'
            // Type 3 started with 0D 0A, then 'NET_NAME=VALUE'
            // The old code checked `fileData[currentPointer] != 0x0D` before the +=2 for Type 2, implying Type 2 might have an extra initial skip.
            if (static_cast<uint8_t>(fileData[currentFileOffset]) != 0x0D) {
                 currentFileOffset += 2; // Skip two unknown bytes for Type 2 (CD BC)
            }
            diodeReadingsType_ = 2;

            while (currentFileOffset < fileData.size()) {
                if (currentFileOffset + 2 > fileData.size()) break;
                if (static_cast<uint8_t>(fileData[currentFileOffset]) != 0x0D || static_cast<uint8_t>(fileData[currentFileOffset+1]) != 0x0A) break; // Expect 0D 0A separator
                currentFileOffset += 2; // Skip 0x0D 0x0A

                if (currentFileOffset >= fileData.size()) break;
                // Check for double 0D0A which might signify end of this section
                if (currentFileOffset + 2 <= fileData.size() && 
                    static_cast<uint8_t>(fileData[currentFileOffset]) == 0x0D && 
                    static_cast<uint8_t>(fileData[currentFileOffset+1]) == 0x0A) {
                    break; 
                }

                std::string netName, value;
                uint32_t startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != '=') currentFileOffset++;
                if (currentFileOffset >= fileData.size()) break;
                netName = readCB2312String(&fileData[startPos], currentFileOffset - startPos);
                currentFileOffset++; // Skip '='

                startPos = currentFileOffset;
                while (currentFileOffset < fileData.size() && static_cast<uint8_t>(fileData[currentFileOffset]) != 0x0D) currentFileOffset++;
                if (currentFileOffset >= fileData.size()) break; // Value should end before 0D (start of next 0D 0A)
                value = readCB2312String(&fileData[startPos], currentFileOffset - startPos);
                
                diodeReadings_[netName]["0"] = value; // Use "0" as pin key for this type
            }
        }
        return true;
    } catch (const std::runtime_error& e) {
        // Log e.what()
        return false;
    }
}

bool PcbLoader::parseNetBlock(const std::vector<char>& fileData, Board& board, uint32_t netDataOffset) {
    if (netDataOffset == 0 || netDataOffset >= fileData.size()) {
        return true; // No net block or invalid offset, considered okay.
    }

    try {
        // Net block starts with its total size (uint32_t)
        if (netDataOffset + 4 > fileData.size()) {
            // Not enough data even for the block size itself
            return false; 
        }
        uint32_t netBlockTotalSize = readLE<uint32_t>(&fileData[netDataOffset]);
        
        uint32_t currentRelativeOffset = 4; // Start reading after the netBlockTotalSize field
        uint32_t netBlockEndOffset = 4 + netBlockTotalSize; // Relative end offset within the conceptual net block data

        while (currentRelativeOffset < netBlockEndOffset) {
            // Each net entry: net_record_size (u32), net_id (u32), net_name (char[net_record_size - 8])
            if (netDataOffset + currentRelativeOffset + 8 > fileData.size()) {
                // Not enough data for net_record_size and net_id fields
                // This indicates a malformed block or premature end.
                return false; 
            }

            uint32_t netRecordSize = readLE<uint32_t>(&fileData[netDataOffset + currentRelativeOffset]);
            uint32_t netId = readLE<uint32_t>(&fileData[netDataOffset + currentRelativeOffset + 4]);

            if (netRecordSize < 8) {
                // Record size must be at least 8 to hold its own size and the ID.
                // If smaller, it's corrupt. Advance past this problematic record as declared.
                // Log_Warning("Invalid net record size %u at offset %u, skipping", netRecordSize, netDataOffset + currentRelativeOffset);
                if (netRecordSize == 0) return false; // Avoid infinite loop if netRecordSize is 0
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
                netName = readCB2312String(&fileData[netDataOffset + currentRelativeOffset + 8], netNameLength);
            }

            // Use emplace to construct Net in place, avoiding issues with default constructor
            board.nets.emplace(static_cast<int>(netId), Net(static_cast<int>(netId), netName));

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

// Implementations for parsePostV6Block, and element-specific parsers (parseArc, parseVia, etc.) will follow.
// ... (make sure this is placed correctly relative to other definitions) ... 
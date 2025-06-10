#include "BoardLoaderFactory.hpp"

#include <algorithm>  // for std::transform
#include <cctype>
#include <cstddef>
#include <functional>
#include <iostream>  // For error messages
#include <memory>
#include <string>
#include <vector>

#include "XZZPCBLoader.hpp"

BoardLoaderFactory::BoardLoaderFactory()
{
    // Register known loaders here
    // For example, the XZZ PCB Loader for .pcb files
    RegisterLoader(".pcb", []() { return std::make_unique<PcbLoader>(); });
    // When you add KiCadLoader, you would add:
    // registerLoader(".kicad_pcb", []() { return std::make_unique<KiCadLoader>(); });
}

void BoardLoaderFactory::RegisterLoader(const std::string& file_extension_hint, std::function<std::unique_ptr<IBoardLoader>()> loader_creator)
{
    std::string lower_extension = file_extension_hint;
    std::transform(lower_extension.begin(), lower_extension.end(), lower_extension.begin(), ::tolower);
    m_loaders_.push_back({lower_extension, std::move(loader_creator)});
}

std::string BoardLoaderFactory::GetFileExtension(const std::string& file_path)
{
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos != std::string::npos) {
        std::string ext = file_path.substr(dot_pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }
    return "";  // No extension
}

std::unique_ptr<Board> BoardLoaderFactory::LoadBoard(const std::string& file_path)
{
    std::string extension = GetFileExtension(file_path);
    if (extension.empty()) {
        std::cerr << "BoardLoaderFactory: File has no extension, cannot determine loader for: " << file_path << std::endl;
        return nullptr;
    }

    for (const auto& entry : m_loaders_) {
        if (entry.file_extension_hint == extension) {
            std::unique_ptr<IBoardLoader> loader = entry.creator();
            if (loader) {
                try {
                    auto board = loader->LoadFromFile(file_path);
                    if (board) {
                        // Initialize the board after loading
                        if (!board->Initialize(file_path)) {
                            std::cerr << "BoardLoaderFactory: Failed to initialize board after loading." << std::endl;
                            return nullptr;
                        }
                    }
                    return board;
                } catch (const std::exception& e) {
                    std::cerr << "BoardLoaderFactory: Error loading board with loader for extension '" << entry.file_extension_hint << "': " << e.what() << std::endl;
                    return nullptr;
                } catch (...) {
                    std::cerr << "BoardLoaderFactory: Unknown error loading board with loader for extension '" << entry.file_extension_hint << "'." << std::endl;
                    return nullptr;
                }
            }
        }
    }

    std::cerr << "BoardLoaderFactory: No loader registered for file extension '" << extension << "' for file: " << file_path << std::endl;
    return nullptr;
}

std::string BoardLoaderFactory::GetSupportedExtensionsFilterString() const
{
    if (m_loaders_.empty()) {
        return "";  // Or a default like ".*" to show all files if no specific loaders
    }
    std::string filter_string;
    for (size_t i = 0; i < m_loaders_.size(); ++i) {
        filter_string += m_loaders_[i].file_extension_hint;
        if (i < m_loaders_.size() - 1) {
            filter_string += ",";
        }
    }
    return filter_string;
}

std::vector<std::string> BoardLoaderFactory::GetSupportedExtensions() const
{
    std::vector<std::string> extensions;
    for (const auto& entry : m_loaders_) {
        extensions.push_back(entry.file_extension_hint);
    }
    return extensions;
}

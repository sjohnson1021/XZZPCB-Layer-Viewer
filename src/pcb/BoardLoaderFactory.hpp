#ifndef BOARD_LOADER_FACTORY_HPP
#define BOARD_LOADER_FACTORY_HPP

#include <functional>
#include <string>
#include <vector>

#include "IBoardLoader.hpp"

class BoardLoaderFactory
{
public:
    BoardLoaderFactory();

    // Registers a loader factory function. The function should return a new instance of a loader.
    // The string parameter is a hint for which files this loader can handle (e.g., file extension like ".pcb").
    void RegisterLoader(const std::string& file_extension_hint, std::function<std::unique_ptr<IBoardLoader>()> loader_creator);

    // Attempts to load a board from the given file path.
    // It will try to find a suitable loader based on file extension or other means.
    std::unique_ptr<Board> LoadBoard(const std::string& file_path);

    // New method to get supported extensions as a comma-separated string for dialogs
    [[nodiscard]] std::string GetSupportedExtensionsFilterString() const;
    // New method to get a list of supported extensions for more granular control (e.g. styling)
    [[nodiscard]] std::vector<std::string> GetSupportedExtensions() const;

private:
    struct LoaderRegistryEntry {
        std::string file_extension_hint;
        std::function<std::unique_ptr<IBoardLoader>()> creator;
        // We might add a function here to check if a loader can handle a file based on its content, not just extension.
        // std::function<bool(const std::string& filePath)> canHandleFile;
    };
    std::vector<LoaderRegistryEntry> m_loaders_ {};

    std::string GetFileExtension(const std::string& file_path);
};

#endif  // BOARD_LOADER_FACTORY_HPP

#ifndef BOARD_LOADER_FACTORY_HPP
#define BOARD_LOADER_FACTORY_HPP

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "IBoardLoader.hpp"
#include "Board.hpp"

class BoardLoaderFactory {
public:
    BoardLoaderFactory();

    // Registers a loader factory function. The function should return a new instance of a loader.
    // The string parameter is a hint for which files this loader can handle (e.g., file extension like ".pcb").
    void registerLoader(const std::string& fileExtensionHint, std::function<std::unique_ptr<IBoardLoader>()> loaderCreator);

    // Attempts to load a board from the given file path.
    // It will try to find a suitable loader based on file extension or other means.
    std::unique_ptr<Board> loadBoard(const std::string& filePath);

    // New method to get supported extensions as a comma-separated string for dialogs
    std::string getSupportedExtensionsFilterString() const;
    // New method to get a list of supported extensions for more granular control (e.g. styling)
    std::vector<std::string> getSupportedExtensions() const;

private:
    struct LoaderRegistryEntry {
        std::string fileExtensionHint;
        std::function<std::unique_ptr<IBoardLoader>()> creator;
        // We might add a function here to check if a loader can handle a file based on its content, not just extension.
        // std::function<bool(const std::string& filePath)> canHandleFile;
    };
    std::vector<LoaderRegistryEntry> m_loaders;

    std::string getFileExtension(const std::string& filePath);
};

#endif // BOARD_LOADER_FACTORY_HPP 
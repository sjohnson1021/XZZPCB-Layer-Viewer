#include "BoardLoaderFactory.hpp"
#include "XZZPCBLoader.hpp" // To register the existing PcbLoader
#include <iostream>         // For error messages
#include <algorithm>        // for std::transform

BoardLoaderFactory::BoardLoaderFactory()
{
    // Register known loaders here
    // For example, the XZZ PCB Loader for .pcb files
    registerLoader(".pcb", []()
                   { return std::make_unique<PcbLoader>(); });
    // When you add KiCadLoader, you would add:
    // registerLoader(".kicad_pcb", []() { return std::make_unique<KiCadLoader>(); });
}

void BoardLoaderFactory::registerLoader(const std::string &fileExtensionHint, std::function<std::unique_ptr<IBoardLoader>()> loaderCreator)
{
    std::string lowerExtension = fileExtensionHint;
    std::transform(lowerExtension.begin(), lowerExtension.end(), lowerExtension.begin(), ::tolower);
    m_loaders.push_back({lowerExtension, std::move(loaderCreator)});
}

std::string BoardLoaderFactory::getFileExtension(const std::string &filePath)
{
    size_t dotPos = filePath.rfind('.');
    if (dotPos != std::string::npos)
    {
        std::string ext = filePath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }
    return ""; // No extension
}

std::unique_ptr<Board> BoardLoaderFactory::loadBoard(const std::string &filePath)
{
    std::string extension = getFileExtension(filePath);
    if (extension.empty())
    {
        std::cerr << "BoardLoaderFactory: File has no extension, cannot determine loader for: " << filePath << std::endl;
        return nullptr;
    }

    for (const auto &entry : m_loaders)
    {
        if (entry.fileExtensionHint == extension)
        {
            std::unique_ptr<IBoardLoader> loader = entry.creator();
            if (loader)
            {
                try
                {
                    auto board = loader->loadFromFile(filePath);
                    if (board)
                    {
                        // Initialize the board after loading
                        if (!board->initialize(filePath))
                        {
                            std::cerr << "BoardLoaderFactory: Failed to initialize board after loading." << std::endl;
                            return nullptr;
                        }
                    }
                    return board;
                }
                catch (const std::exception &e)
                {
                    std::cerr << "BoardLoaderFactory: Error loading board with loader for extension '"
                              << entry.fileExtensionHint << "': " << e.what() << std::endl;
                    return nullptr;
                }
                catch (...)
                {
                    std::cerr << "BoardLoaderFactory: Unknown error loading board with loader for extension '"
                              << entry.fileExtensionHint << "'." << std::endl;
                    return nullptr;
                }
            }
        }
    }

    std::cerr << "BoardLoaderFactory: No loader registered for file extension '" << extension << "' for file: " << filePath << std::endl;
    return nullptr;
}

std::string BoardLoaderFactory::getSupportedExtensionsFilterString() const
{
    if (m_loaders.empty())
    {
        return ""; // Or a default like ".*" to show all files if no specific loaders
    }
    std::string filter_string;
    for (size_t i = 0; i < m_loaders.size(); ++i)
    {
        filter_string += m_loaders[i].fileExtensionHint;
        if (i < m_loaders.size() - 1)
        {
            filter_string += ",";
        }
    }
    return filter_string;
}

std::vector<std::string> BoardLoaderFactory::getSupportedExtensions() const
{
    std::vector<std::string> extensions;
    for (const auto &entry : m_loaders)
    {
        extensions.push_back(entry.fileExtensionHint);
    }
    return extensions;
}
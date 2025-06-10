#ifndef IBOARD_LOADER_HPP
#define IBOARD_LOADER_HPP

#include <string>
#include <memory>

// Forward declaration
class Board;

class IBoardLoader {
public:
    virtual ~IBoardLoader() = default;
    virtual std::unique_ptr<Board> LoadFromFile(const std::string& file_path) = 0;
};

#endif // IBOARD_LOADER_HPP 
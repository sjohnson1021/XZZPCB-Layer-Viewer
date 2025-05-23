#ifndef IBOARD_LOADER_HPP
#define IBOARD_LOADER_HPP

#include <string>
#include <memory>
#include "Board.hpp" // Assuming Board.hpp is in the same directory or accessible

// Forward declaration
class Board;

class IBoardLoader {
public:
    virtual ~IBoardLoader() = default;
    virtual std::unique_ptr<Board> loadFromFile(const std::string& filePath) = 0;
};

#endif // IBOARD_LOADER_HPP 
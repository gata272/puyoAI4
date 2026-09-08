#pragma once

#include <array>
#include <cstdint>

namespace puyo {

constexpr int BOARD_WIDTH = 6;
constexpr int BOARD_HEIGHT = 14;
constexpr int VISIBLE_HEIGHT = 12;

enum class Cell : std::uint8_t {
    Empty = 0,
    Red = 1,
    Blue = 2,
    Green = 3,
    Yellow = 4,
    Garbage = 5
};

class Board {
public:
    Board();

    void clear();

    Cell get(int x, int y) const;
    void set(int x, int y, Cell value);

    int height(int x) const;
    std::array<int, BOARD_WIDTH> heights() const;

    bool gameOver() const;

    const std::array<std::array<Cell, BOARD_HEIGHT>, BOARD_WIDTH>& cells() const {
        return cells_;
    }

private:
    // [x][y], y=0 is the bottom row.
    std::array<std::array<Cell, BOARD_HEIGHT>, BOARD_WIDTH> cells_{};
};

} // namespace puyo

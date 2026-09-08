#include "board.h"

namespace puyo {

Board::Board() {
    clear();
}

void Board::clear() {
    for (auto& column : cells_) {
        column.fill(Cell::Empty);
    }
}

Cell Board::get(int x, int y) const {
    if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT) {
        return Cell::Garbage;
    }
    return cells_[x][y];
}

void Board::set(int x, int y, Cell value) {
    if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT) return;
    cells_[x][y] = value;
}

int Board::height(int x) const {
    if (x < 0 || x >= BOARD_WIDTH) return BOARD_HEIGHT;
    for (int y = BOARD_HEIGHT - 1; y >= 0; --y) {
        if (cells_[x][y] != Cell::Empty) return y + 1;
    }
    return 0;
}

std::array<int, BOARD_WIDTH> Board::heights() const {
    std::array<int, BOARD_WIDTH> h{};
    for (int x = 0; x < BOARD_WIDTH; ++x) h[x] = height(x);
    return h;
}

bool Board::gameOver() const {
    // The simulator mirrors the game’s 2nd hidden column danger area.
    return height(2) >= VISIBLE_HEIGHT;
}

} // namespace puyo

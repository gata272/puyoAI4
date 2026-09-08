#pragma once

#include "board.h"
#include "../search/move.h"

namespace puyo {

struct SimulationResult {
    Board board;
    int chains = 0;
    int score = 0;
    int erased = 0;
    bool allClear = false;
    bool gameOver = false;
};

class Simulator {
public:
    static SimulationResult drop(
        const Board& board,
        const PuyoPair& pair,
        const Move& move
    );

    static bool canPlace(
        const Board& board,
        const PuyoPair& pair,
        int x,
        int y,
        int rotation
    );

    static int findDropY(
        const Board& board,
        const PuyoPair& pair,
        int x,
        int rotation
    );

private:
    static void gravity(Board& board);
    static int resolve(Board& board, int& score, int& erased);
};

} // namespace puyo

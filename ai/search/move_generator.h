#pragma once

#include <vector>
#include "move.h"
#include "../simulation/board.h"
#include "../simulation/simulator.h"

namespace puyo {

std::vector<Move> generateLegalMoves(
    const Board& board,
    const PuyoPair& pair
);

} // namespace puyo

#pragma once

#include "../simulation/board.h"
#include "../search/move.h"
#include <vector>

namespace puyo {

// Estimates how much latent long-chain structure remains without revealing
// any hidden future pieces.  This is intentionally a static potential, not a
// claim that the board can immediately fire for this many chains.
double longChainPotential(
    const Board& board,
    const std::vector<PuyoPair>& lookahead
);

} // namespace puyo

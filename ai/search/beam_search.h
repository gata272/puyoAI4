#pragma once

#include <vector>
#include "../simulation/board.h"
#include "../evaluation/weights.h"
#include "move.h"

namespace puyo {

class BeamSearch {
public:
    Move chooseMove(
        const Board& board,
        const std::vector<PuyoPair>& pieces,
        const Weights& weights,
        int depth = 3,
        int beamWidth = 8
    ) const;
};

} // namespace puyo

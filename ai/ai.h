#pragma once

#include "gtr/gtr_ai.h"
#include "search/beam_search.h"
#include "evaluation/weights.h"
#include "simulation/board.h"

#include <string>
#include <vector>

namespace puyo {

class AI {
public:
    AI();

    void reset();

    Move chooseMove(
        int turn,
        const Board& board,
        const std::vector<PuyoPair>& pieces
    );

    // Configurable entry point used by benchmarks and research tooling.
    // Production gameplay continues to use chooseMove() above.
    Move chooseMove(
        int turn,
        const Board& board,
        const std::vector<PuyoPair>& pieces,
        int depth,
        int beamWidth
    );

    const char* patternName() const;

private:
    gtr::GtrAI gtr_;
    BeamSearch search_;
    Weights weights_;
    std::string patternName_;
};

} // namespace puyo

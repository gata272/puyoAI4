#pragma once

#include "../simulation/board.h"

namespace puyo {

// Fast, information-honest virtual-fire probe inspired by the v13 chain
// builder family: all 4x4 colour-pair types are tested, but only the two
// canonical orientations are needed because the ordered colour pairs make
// the opposite orientations equivalent.
struct VirtualChainFeatures {
    int bestChain = 0;
    int top3ChainSum = 0;
    int count2Plus = 0;
    int count3Plus = 0;
    int resultCount = 0;
    int bestScore = 0;
    int top3ScoreSum = 0;
};

VirtualChainFeatures analyzeVirtualChainPotential(const Board& board);

// Converts the raw probe metrics to the search's score scale. This is a
// potential estimate, not a prediction that a hidden future piece exists.
double virtualChainPotentialScore(const VirtualChainFeatures& f, const Board& board);

} // namespace puyo

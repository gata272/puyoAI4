#pragma once

#include "../simulation/board.h"
#include "../search/move.h"
#include <vector>

namespace puyo {

// A compact description of the long-chain "blueprint" present in a board.
// The blueprint is a sequential dependency path, not a branching score:
// one removal should make the next removal possible.  Nodes are individual
// board groups/events, so A -> B -> A is valid.
struct ChainBlueprint {
    int longestPath = 0;
    int completedLinks = 0;
    int latentLinks = 0;
    int reusableColorLinks = 0;
    int fragileLinks = 0;
    int preparedTriples = 0;
    int preparedPairs = 0;
    double score = 0.0;
};

ChainBlueprint analyzeChainBlueprint(
    const Board& board,
    const std::vector<PuyoPair>& lookahead
);

ChainBlueprint quickChainBlueprint(
    const Board& board,
    const std::vector<PuyoPair>& lookahead
);

double chainBlueprintScore(
    const Board& board,
    const std::vector<PuyoPair>& lookahead
);

} // namespace puyo

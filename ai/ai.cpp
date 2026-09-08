#include "ai.h"

#include <algorithm>

namespace puyo {

AI::AI()
    : weights_(amaBuildWeights()) {
}

void AI::reset() {
    gtr_.reset();
    patternName_.clear();
}

Move AI::chooseMove(
    int turn,
    const Board& board,
    const std::vector<PuyoPair>& pieces
) {
    return chooseMove(turn, board, pieces, 3, 8);
}

Move AI::chooseMove(
    int turn,
    const Board& board,
    const std::vector<PuyoPair>& pieces,
    int depth,
    int beamWidth
) {
    if (pieces.empty()) return {-1, 0, false};

    // Preserve the current AI's first three GTR moves. Once the GTR plan is
    // unavailable or exhausted, switch to the general search/evaluation engine.
    if (turn >= 0 && turn < 3 && pieces.size() >= 3) {
        Move gtrMove = gtr_.chooseMove(
            turn,
            pieces[0],
            pieces[1],
            pieces[2]
        );

        patternName_ = gtr_.patternName();

        if (gtrMove.valid) {
            return gtrMove;
        }
    }

    patternName_.clear();

    return search_.chooseMove(
        board,
        pieces,
        weights_,
        std::max(1, depth),
        std::max(1, beamWidth)
    );
}

const char* AI::patternName() const {
    return patternName_.c_str();
}

} // namespace puyo

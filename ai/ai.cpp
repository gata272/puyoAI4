#include "ai.h"
#include "simulation/simulator.h"
#include "search/move_generator.h"

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
    return chooseMove(turn, board, pieces, 3, 12);
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
            // Keep GTR when it is safe. If the planned GTR placement would
            // itself cause game over while another safe placement exists,
            // fall through to the general search instead. If no safe move
            // exists, the general search has an explicit death-placement
            // fallback and will return the least-bad game-over move.
            const auto legal = generateLegalMoves(board, pieces[0]);
            bool safeExists = false;
            for (const auto& move : legal) {
                const auto sim = Simulator::drop(board, pieces[0], move);
                if (!sim.gameOver || sim.allClear) {
                    safeExists = true;
                    break;
                }
            }
            const auto gtrSim = Simulator::drop(board, pieces[0], gtrMove);
            if (!safeExists || !gtrSim.gameOver || gtrSim.allClear) {
                return gtrMove;
            }
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

void AI::resetWeights() {
    weights_ = amaBuildWeights();
}

bool AI::setWeight(int index, double value) {
    return puyo::setWeight(weights_, index, value);
}

double AI::getWeight(int index) const {
    return puyo::getWeight(weights_, index);
}

int AI::weightCount() const {
    return puyo::weightCount();
}

const char* AI::weightName(int index) const {
    return puyo::weightName(index);
}

const char* AI::patternName() const {
    return patternName_.c_str();
}

} // namespace puyo

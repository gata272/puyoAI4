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
    // Wider beam than the original default (12) so a quiet, high-potential
    // construction is less likely to be pruned away before its payoff (a
    // larger later chain) becomes visible to the search. 20 was chosen as a
    // balance between search quality and per-move think time in the browser
    // (WASM); see docs/RESEARCH_NEXT.md ("PuyoAI12") for the benchmark data
    // behind this change, and config/search.json for the wider beamWidth
    // (32) recommended for offline chain_benchmark_cli runs where think
    // time does not need to stay real-time.
    return chooseMove(turn, board, pieces, 3, 20);
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

const char* AI::patternName() const {
    return patternName_.c_str();
}

} // namespace puyo

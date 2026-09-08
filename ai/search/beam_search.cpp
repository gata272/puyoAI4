#include "beam_search.h"

#include "move_generator.h"
#include "../evaluation/evaluation.h"
#include "../simulation/simulator.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace puyo {

namespace {

struct Node {
    Board board;
    double score = 0.0;
    Move root;
};

double searchNode(
    const Board& board,
    const std::vector<PuyoPair>& pieces,
    int depth,
    int maxDepth,
    int beamWidth,
    const Weights& weights,
    const Move& rootMove
) {
    if (depth >= maxDepth || depth >= static_cast<int>(pieces.size())) {
        EvaluationContext ctx;
        if (depth < static_cast<int>(pieces.size())) {
            ctx.lookahead.assign(pieces.begin() + depth, pieces.end());
        }
        ctx.quiescenceDepth = 3;
        return evaluate(board, weights, ctx);
    }

    const auto moves = generateLegalMoves(board, pieces[depth]);
    if (moves.empty()) return -1e15;

    std::vector<Node> candidates;
    candidates.reserve(moves.size());

    for (const Move& move : moves) {
        SimulationResult sim =
            Simulator::drop(board, pieces[depth], move);

        if (sim.gameOver && !sim.allClear) continue;

        EvaluationContext ctx;
        if (depth + 1 < static_cast<int>(pieces.size())) {
            ctx.lookahead.assign(
                pieces.begin() + depth + 1,
                pieces.end()
            );
        }
        ctx.quiescenceDepth = (depth + 1 >= maxDepth) ? 3 : 0;

        double score =
            evaluate(sim.board, weights, ctx) +
            actionPenalty(board, sim, move, weights);

        candidates.push_back({
            sim.board,
            score,
            depth == 0 ? move : rootMove
        });
    }

    if (candidates.empty()) return -1e15;

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const Node& a, const Node& b) {
            return a.score > b.score;
        }
    );

    if (static_cast<int>(candidates.size()) > beamWidth) {
        candidates.resize(beamWidth);
    }

    double best = -1e15;

    for (const auto& candidate : candidates) {
        double value = candidate.score;

        if (depth + 1 < maxDepth &&
            depth + 1 < static_cast<int>(pieces.size())) {
            value += 0.85 * searchNode(
                candidate.board,
                pieces,
                depth + 1,
                maxDepth,
                beamWidth,
                weights,
                candidate.root
            );
        }

        best = std::max(best, value);
    }

    return best;
}

Move chooseRoot(
    const Board& board,
    const std::vector<PuyoPair>& pieces,
    const Weights& weights,
    int maxDepth,
    int beamWidth
) {
    if (pieces.empty()) return {-1, 0, false};

    const auto moves = generateLegalMoves(board, pieces[0]);

    double bestScore = -1e15;
    Move best{-1, 0, false};

    for (const Move& move : moves) {
        SimulationResult sim =
            Simulator::drop(board, pieces[0], move);

        if (sim.gameOver && !sim.allClear) continue;

        EvaluationContext ctx;
        if (pieces.size() > 1) {
            ctx.lookahead.assign(pieces.begin() + 1, pieces.end());
        }
        ctx.quiescenceDepth = 3;

        double score =
            evaluate(sim.board, weights, ctx) +
            actionPenalty(board, sim, move, weights);

        if (pieces.size() > 1) {
            score += 0.85 * searchNode(
                sim.board,
                pieces,
                1,
                maxDepth,
                beamWidth,
                weights,
                move
            );
        }

        if (score > bestScore) {
            bestScore = score;
            best = move;
        }
    }

    return best;
}

} // namespace

Move BeamSearch::chooseMove(
    const Board& board,
    const std::vector<PuyoPair>& pieces,
    const Weights& weights,
    int depth,
    int beamWidth
) const {
    return chooseRoot(
        board,
        pieces,
        weights,
        depth,
        beamWidth
    );
}

} // namespace puyo

#include "survival_horizon.h"

#include "../search/move_generator.h"
#include "../simulation/simulator.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace puyo {

SurvivalHorizon analyzeSurvivalHorizon(
    const Board& board,
    const PuyoPair* next,
    const PuyoPair* nextNext
) {
    SurvivalHorizon out;
    if (!next) return out;

    const auto moves = generateLegalMoves(board, *next);
    out.geometricMoves = static_cast<int>(moves.size());
    if (moves.empty()) {
        out.safeMoves = 0;
        out.bestNextGeometricMoves = nextNext ? 0 : -1;
        return out;
    }

    const auto h = board.heights();
    out.safeMoves = 0;
    out.bestNextGeometricMoves = -1;
    std::vector<std::pair<Move, int>> safeLandingMoves;
    safeLandingMoves.reserve(moves.size());

    for (const Move& move : moves) {
        const int y = Simulator::findDropY(board, *next, move.x, move.rotation);
        if (y < 0) continue;

        int landingHeight2 = h[2];
        switch (move.rotation & 3) {
            case 0: // main at x, sub above
                if (move.x == 2) landingHeight2 = std::max(landingHeight2, y + 2);
                break;
            case 1: // sub left
                if (move.x == 2 || move.x - 1 == 2)
                    landingHeight2 = std::max(landingHeight2, y + 1);
                break;
            case 2: // sub below
                if (move.x == 2) landingHeight2 = std::max(landingHeight2, y + 1);
                break;
            case 3: // sub right
                if (move.x == 2 || move.x + 1 == 2)
                    landingHeight2 = std::max(landingHeight2, y + 1);
                break;
        }

        if (landingHeight2 >= VISIBLE_HEIGHT) continue;
        ++out.safeMoves;
        if (nextNext) safeLandingMoves.push_back({move, y});
    }

    // Only inspect the second geometric horizon when the first horizon is
    // already narrow. This keeps the correction cheap on healthy boards.
    if (nextNext && out.safeMoves <= 2) {
        out.bestNextGeometricMoves = 0;
        for (const auto& entry : safeLandingMoves) {
            const Move& move = entry.first;
            const int y = entry.second;
            Board projected = board;
            switch (move.rotation & 3) {
                case 0:
                    projected.set(move.x, y, static_cast<Cell>(next->main));
                    projected.set(move.x, y + 1, static_cast<Cell>(next->sub));
                    break;
                case 1:
                    projected.set(move.x, y, static_cast<Cell>(next->main));
                    projected.set(move.x - 1, y, static_cast<Cell>(next->sub));
                    break;
                case 2:
                    projected.set(move.x, y, static_cast<Cell>(next->main));
                    projected.set(move.x, y - 1, static_cast<Cell>(next->sub));
                    break;
                case 3:
                    projected.set(move.x, y, static_cast<Cell>(next->main));
                    projected.set(move.x + 1, y, static_cast<Cell>(next->sub));
                    break;
            }
            out.bestNextGeometricMoves = std::max(
                out.bestNextGeometricMoves,
                static_cast<int>(generateLegalMoves(projected, *nextNext).size())
            );
        }
    }

    return out;
}

double survivalHorizonScore(
    const SurvivalHorizon& horizon,
    int previousSafeMoves
) {
    if (horizon.safeMoves < 0) return 0.0;

    // Survival is a correction signal, not a general preference for empty
    // space. Comfortable mobility (4+) receives almost no positive reward;
    // the score becomes strongly negative only in the collapse zone.
    static constexpr double mobility[] = {
        -52000.0, // 0
        -22000.0, // 1
        -10000.0, // 2
        -4000.0,  // 3
        -800.0,   // 4
        0.0,      // 5
        0.0,      // 6
        0.0       // 7+
    };
    const int safeIndex = std::min(horizon.safeMoves, 7);
    double score = mobility[safeIndex];

    if (horizon.bestNextGeometricMoves >= 0 && horizon.safeMoves <= 2) {
        score += std::clamp(
            static_cast<double>(horizon.bestNextGeometricMoves - 8) * 300.0,
            -3000.0,
            3000.0
        );
    }

    if (previousSafeMoves >= 0) {
        const int collapse = previousSafeMoves - horizon.safeMoves;
        if (collapse >= 4) score -= 15000.0;
        else if (collapse == 3) score -= 9000.0;
        else if (collapse == 2) score -= 3500.0;
        else if (collapse == 1) score -= 800.0;
    }

    return std::clamp(score, -70000.0, 0.0);
}

} // namespace puyo

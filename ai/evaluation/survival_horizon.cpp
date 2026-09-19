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
        out.bestNextSafeMoves = nextNext ? 0 : -1;
        return out;
    }

    const auto h = board.heights();
    const int maxHeight = *std::max_element(h.begin(), h.end());
    out.safeMoves = 0;

    // Most of the time a cheap landing-height test is sufficient. Once the
    // board is near the danger line, however, resolve the candidate exactly.
    // This matters because a trigger can clear puyos and make an apparently
    // dangerous landing safe; the old probe counted such a move as unsafe.
    const bool exactSafety = h[2] >= 10 || (maxHeight >= 13 && h[2] >= 9) || out.geometricMoves <= 4;
    std::vector<Board> safeBoards;
    if (nextNext) safeBoards.reserve(moves.size());

    for (const Move& move : moves) {
        bool safe = false;
        Board projected;

        if (exactSafety) {
            const SimulationResult sim = Simulator::drop(board, *next, move);
            safe = !sim.gameOver || sim.allClear;
            if (safe && nextNext) projected = sim.board;
        } else {
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
            safe = landingHeight2 < VISIBLE_HEIGHT;

            if (safe && nextNext) {
                projected = board;
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
            }
        }

        if (!safe) continue;
        ++out.safeMoves;
        if (nextNext) safeBoards.push_back(std::move(projected));
    }

    // Only inspect the second geometric horizon when the first horizon is
    // narrow. On exact-safety boards this uses the resolved post-chain board,
    // so a useful trigger-clearing move is not unfairly penalized.
    if (nextNext && out.safeMoves <= 4) {
        out.bestNextGeometricMoves = 0;
        out.bestNextSafeMoves = 0;
        for (const Board& projected : safeBoards) {
            const auto nextMoves = generateLegalMoves(projected, *nextNext);
            out.bestNextGeometricMoves = std::max(
                out.bestNextGeometricMoves,
                static_cast<int>(nextMoves.size())
            );

            // Use the exact simulator here.  The cheap first-horizon probe can
            // deliberately avoid resolution on healthy boards, but a
            // two-step escape signal must not call a trigger-clearing move
            // "unsafe" merely because gravity/chain resolution was omitted.
            int safeSecond = 0;
            for (const Move& move : nextMoves) {
                const SimulationResult sim =
                    Simulator::drop(projected, *nextNext, move);
                if (!sim.gameOver || sim.allClear) ++safeSecond;
            }
            out.bestNextSafeMoves = std::max(out.bestNextSafeMoves, safeSecond);
        }
    }

    return out;
}

double survivalHorizonScore(
    const SurvivalHorizon& horizon,
    int previousSafeMoves,
    int previousGeometricMoves
) {
    if (horizon.safeMoves < 0) return 0.0;

    // Survival is a correction signal, not a general preference for empty
    // space. Comfortable mobility receives no reward. The negative region is
    // concentrated near collapse so the chain evaluator remains dominant.
    static constexpr double safePenalty[] = {
        -120000.0, // 0
        -50000.0,  // 1
        -18000.0,  // 2
        -4500.0,   // 3
        0.0,       // 4
        0.0,       // 5
        0.0,       // 6
        0.0        // 7+
    };
    const int safeIndex = std::min(horizon.safeMoves, 7);
    double score = safePenalty[safeIndex];


    if (horizon.bestNextGeometricMoves >= 0 && horizon.safeMoves <= 4) {
        score += std::clamp(
            static_cast<double>(horizon.bestNextGeometricMoves - 8) * 300.0,
            -3000.0,
            3000.0
        );
    }

    // Prefer an actual escape route, not merely a board on which the next
    // piece happens to fit geometrically.  This term is only meaningful in
    // the narrow first-horizon region, so it cannot turn healthy construction
    // into a generic "maximize mobility" policy.
    if (horizon.bestNextSafeMoves >= 0 && horizon.safeMoves <= 4) {
        score += std::clamp(
            static_cast<double>(horizon.bestNextSafeMoves - 3) * 1200.0,
            -6000.0,
            4500.0
        );
    }

    if (previousSafeMoves >= 0) {
        const int collapse = previousSafeMoves - horizon.safeMoves;
        if (collapse >= 4) score -= 9000.0;
        else if (collapse == 3) score -= 5500.0;
        else if (collapse == 2) score -= 1800.0;
        else if (collapse == 1) score -= 250.0;
    }

    if (previousGeometricMoves >= 0) {
        const int geometricCollapse = previousGeometricMoves - horizon.geometricMoves;
        if (geometricCollapse >= 5) score -= 5000.0;
        else if (geometricCollapse >= 3) score -= 2500.0;
        else if (geometricCollapse == 2) score -= 900.0;
        else if (geometricCollapse == 1) score -= 150.0;
    }

    return std::clamp(score, -150000.0, 0.0);
}

} // namespace puyo

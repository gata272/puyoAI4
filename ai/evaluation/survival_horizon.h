#pragma once

#include "../simulation/board.h"
#include "../search/move.h"

namespace puyo {

struct SurvivalHorizon {
    // Number of placements of `next` that do not immediately enter game over.
    // -1 means that no next pair was supplied.
    int geometricMoves = -1;
    int safeMoves = -1;

    // Among safe placements of `next`, the largest number of geometric
    // placements available for `nextNext`.
    int bestNextGeometricMoves = -1;

    // A stronger two-step signal than geometric mobility alone.  We count the
    // largest number of actually safe placements for nextNext from any safe
    // next placement.  This is only evaluated when the first horizon is
    // narrow, so it remains a cheap escape-route probe rather than a second
    // full search.
    int bestNextSafeMoves = -1;
};

SurvivalHorizon analyzeSurvivalHorizon(
    const Board& board,
    const PuyoPair* next,
    const PuyoPair* nextNext = nullptr
);

// Returns a bounded utility in score units. The curve is intentionally steep
// around 0-2 safe moves: a rapid loss of future mobility is a warning signal,
// not a reason to abandon a strong long-chain construction at comfortable
// mobility levels.
double survivalHorizonScore(
    const SurvivalHorizon& horizon,
    int previousSafeMoves = -1,
    int previousGeometricMoves = -1
);

} // namespace puyo

#include "ai/evaluation/survival_horizon.h"
#include "ai/simulation/board.h"
#include <cassert>
#include <iostream>

using namespace puyo;

int main() {
    Board empty;
    PuyoPair next{1, 2};
    PuyoPair nextNext{3, 4};
    const auto open = analyzeSurvivalHorizon(empty, &next, &nextNext);
    assert(open.geometricMoves > 0);
    assert(open.safeMoves == open.geometricMoves);
    assert(open.bestNextGeometricMoves == -1);

    // A fully occupied danger column must leave no safe placement for a pair
    // whose main puyo is anchored there. Other columns may still be legal.
    Board danger;
    for (int y = 0; y < VISIBLE_HEIGHT; ++y)
        danger.set(2, y, Cell::Blue);
    const auto d = analyzeSurvivalHorizon(danger, &next, nullptr);
    assert(d.geometricMoves > 0);
    assert(d.safeMoves >= 0);
    assert(d.safeMoves <= d.geometricMoves);

    assert(survivalHorizonScore({0, 0, 0}, 5) <
           survivalHorizonScore({5, 6, 12}, 5));
    assert(survivalHorizonScore({2, 5, 8}, 6) <
           survivalHorizonScore({5, 8, 12}, 6));

    std::cout << "survival horizon tests passed\n";
    return 0;
}

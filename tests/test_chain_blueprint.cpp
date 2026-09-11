#include "ai/evaluation/chain_blueprint.h"
#include "ai/simulation/board.h"

#include <cassert>
#include <iostream>

using namespace puyo;

static void set(Board& b, int x, int y, int c) {
    b.set(x, y, static_cast<Cell>(c));
}

int main() {
    // B is a 3-group directly above the A trigger. Removing B exposes four A.
    Board vertical;
    set(vertical,0,0,1); set(vertical,1,0,1); set(vertical,2,0,1);
    set(vertical,0,1,2); set(vertical,0,2,2); set(vertical,0,3,2);
    const auto v = analyzeChainBlueprint(vertical, {});
    assert(v.longestPath >= 1);

    // Two separate A prepared groups are allowed: the same colour can occur
    // twice in a sequential blueprint (A -> B -> A), rather than being a
    // colour-level cycle that gets rejected.
    Board reuse;
    set(reuse,0,0,1); set(reuse,1,0,1); set(reuse,2,0,1);
    set(reuse,0,1,2); set(reuse,0,2,2); set(reuse,0,3,2);
    set(reuse,4,1,1); set(reuse,5,1,1); set(reuse,4,2,1);
    const auto r = analyzeChainBlueprint(reuse, {});
    assert(r.reusableColorLinks >= 1);

    // No branch bonus is represented by the API: score is driven by the
    // longest sequential path plus small latent/preparation terms.
    assert(r.score >= 0.0);

    std::cout << "chain blueprint tests passed\n";
    return 0;
}

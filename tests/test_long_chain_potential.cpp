#include "../ai/evaluation/long_chain_potential.h"
#include "../ai/evaluation/features.h"
#include "../ai/simulation/board.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace puyo;

    Board empty;
    const double emptyPotential = longChainPotential(empty, {});
    assert(emptyPotential == 0.0);

    // A 3-group with a reachable top extension must score above an isolated
    // sparse board. This guards the main "3 now, 4 next" construction signal.
    Board triple;
    triple.set(1,0,Cell::Red);
    triple.set(1,1,Cell::Red);
    triple.set(2,0,Cell::Red);
    const double triplePotential = longChainPotential(triple, {{1,2},{3,4}});
    assert(triplePotential > 0.0);

    // A 4-group is already fireable and should not receive the same latent
    // extension bonus as an un-fired 3-group.
    Board four;
    four.set(1,0,Cell::Red);
    four.set(1,1,Cell::Red);
    four.set(2,0,Cell::Red);
    four.set(2,1,Cell::Red);
    const double fourPotential = longChainPotential(four, {{1,2},{3,4}});
    assert(triplePotential > fourPotential);

    // A reachable queue color must contribute to potential compatibility.
    const double withoutQueue = longChainPotential(triple, {});
    const double withQueue = longChainPotential(triple, {{1,2},{3,4}});
    assert(withQueue > withoutQueue);

    std::cout << "long-chain potential tests passed: "
              << triplePotential << " > " << fourPotential << "\n";
    return 0;
}

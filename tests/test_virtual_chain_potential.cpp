#include "../ai/evaluation/virtual_chain_potential.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace puyo;

int main() {
    // B is a latent trigger and A is a 3+1 tail. A virtual two-puyo probe
    // should be able to discover the resulting two-wave chain even though no
    // real future pair is being assumed.
    Board relay;
    relay.set(0, 0, Cell::Blue);
    relay.set(0, 1, Cell::Blue);
    relay.set(0, 2, Cell::Blue);
    relay.set(1, 0, Cell::Red);
    relay.set(1, 1, Cell::Red);
    relay.set(1, 2, Cell::Red);
    relay.set(0, 3, Cell::Red);

    const VirtualChainFeatures f = analyzeVirtualChainPotential(relay);
    assert(f.bestChain >= 2);
    assert(f.top3ChainSum >= f.bestChain);
    assert(f.count2Plus >= 1);
    assert(f.count3Plus >= 0);
    assert(f.bestScore > 0);
    assert(std::isfinite(virtualChainPotentialScore(f, relay)));

    // Empty/very small boards intentionally skip the expensive virtual probe.
    Board empty;
    const VirtualChainFeatures e = analyzeVirtualChainPotential(empty);
    assert(e.bestChain == 0);
    assert(e.resultCount == 0);

    std::cout << "virtual-chain potential tests passed: "
              << f.bestChain << "," << f.top3ChainSum << "\n";
    return 0;
}

#include "../ai/evaluation/trigger_route.h"
#include "../ai/evaluation/long_chain_potential.h"
#include <cassert>
#include <iostream>

using namespace puyo;

int main() {
    // B is a latent trigger (exactly three B).  A is a 3+1 arrangement:
    // before B fires, A is only three connected; after B disappears and the
    // column falls, the fourth A joins it and A becomes a fireable group.
    Board relay;
    relay.set(0,0,Cell::Blue);
    relay.set(0,1,Cell::Blue);
    relay.set(0,2,Cell::Blue);
    relay.set(1,0,Cell::Red);
    relay.set(1,1,Cell::Red);
    relay.set(1,2,Cell::Red);
    relay.set(0,3,Cell::Red);

    assert(triggerRouteLength(relay) >= 2);
    assert(preparedGroupScore(relay) > 0.0);

    // A post-trigger tail exists: the A group is not fireable in the original
    // state, but becomes fireable after the hypothetical B trigger is removed.
    assert(triggerRelayScore(relay) > 0.0);
    assert(postTriggerTailScore(relay) > 0.0);

    // A real already-fired group should not receive a large latent-route score
    // merely because it is large.
    Board fired;
    fired.set(0,0,Cell::Red);
    fired.set(1,0,Cell::Red);
    fired.set(2,0,Cell::Red);
    fired.set(3,0,Cell::Red);
    const double firedPotential = longChainPotential(fired, {});
    assert(firedPotential < 100000.0);

    // The construction board with a relay route must score above the trivial
    // four-group board for latent large-chain structure.
    assert(longChainPotential(relay, {}) > firedPotential);

    std::cout << "trigger structure tests passed\n";
    return 0;
}

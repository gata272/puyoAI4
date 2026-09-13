#include "ai/evaluation/trigger_route.h"
#include "ai/simulation/board.h"
#include <cassert>
#include <iostream>

using namespace puyo;

int main() {
    // B is the trigger. Removing B makes A four, then removing A makes C four.
    // This is deliberately a sequential dependency, not A->B->C by colour
    // identity; repeated colours are legal in the route model.
    Board b;
    b.set(1,0,Cell::Red); b.set(2,0,Cell::Red); b.set(3,0,Cell::Red);
    b.set(1,1,Cell::Blue); b.set(2,1,Cell::Blue); b.set(1,2,Cell::Blue);
    b.set(2,2,Cell::Red);
    b.set(0,3,Cell::Green); b.set(1,3,Cell::Green); b.set(0,4,Cell::Green);
    b.set(1,4,Cell::Blue);
    assert(triggerRouteLength(b) >= 3);
    assert(chainDependencyPathScore(b) > 0.0);

    // A lone prepared triple is a route of one, not a phantom multi-chain.
    Board single;
    single.set(0,0,Cell::Red);
    single.set(1,0,Cell::Red);
    single.set(2,0,Cell::Red);
    assert(triggerRouteLength(single) == 1);


    std::cout << "chain dependency tests passed\n";
    return 0;
}

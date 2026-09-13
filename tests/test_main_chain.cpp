#include "../ai/evaluation/main_chain.h"
#include <cassert>
#include <iostream>

using namespace puyo;

int main() {
    // B trigger -> A: removing BBB makes the upper A join AAA.
    Board b;
    b.set(1,0,Cell::Red); b.set(2,0,Cell::Red); b.set(3,0,Cell::Red);
    b.set(1,1,Cell::Blue); b.set(2,1,Cell::Blue); b.set(1,2,Cell::Blue);
    b.set(2,2,Cell::Red);
    const auto plan = analyzeMainChain(b);
    assert(plan.length() >= 2);
    assert(plan.colors[0] == static_cast<int>(Cell::Blue));
    assert(plan.colors[1] == static_cast<int>(Cell::Red));

    MainChainPlan p;
    p.colors = {2, 1, 3};
    MainChainPlan extended;
    extended.colors = {2, 1, 3, 4};
    assert(mainChainContinuityScore(p, extended, 0) > 0.0);

    MainChainPlan abandoned;
    abandoned.colors = {4, 3, 1};
    assert(mainChainContinuityScore(p, abandoned, 0) < 0.0);

    std::cout << "main chain policy tests passed\n";
    return 0;
}

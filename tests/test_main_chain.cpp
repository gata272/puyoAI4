#include "../ai/evaluation/main_chain.h"
#include <cassert>
#include <cmath>
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

    // Spatial policy: a central trigger with available workspace should not
    // be scored below an otherwise identical edge trigger.
    Board central;
    central.set(2,0,Cell::Red);
    central.set(3,0,Cell::Red);
    central.set(2,1,Cell::Red);
    MainChainPlan centralPlan;
    centralPlan.colors = {static_cast<int>(Cell::Red), static_cast<int>(Cell::Blue)};
    centralPlan.anchorX = 2;
    centralPlan.anchorY = 0;

    Board edge;
    edge.set(0,0,Cell::Red);
    edge.set(0,1,Cell::Red);
    edge.set(1,0,Cell::Red);
    MainChainPlan edgePlan = centralPlan;
    edgePlan.anchorX = 0;
    edgePlan.anchorY = 0;

    assert(mainChainConstructionScore(central, centralPlan) >
           mainChainConstructionScore(edge, edgePlan));

    // The construction score must stay finite on a full-height board.
    Board full;
    for (int x = 0; x < BOARD_WIDTH; ++x)
        for (int y = 0; y < VISIBLE_HEIGHT; ++y)
            full.set(x, y, Cell::Red);
    assert(std::isfinite(mainChainConstructionScore(full, centralPlan)));
    assert(std::isfinite(prematureMainChainTriggerRisk(full, centralPlan)));

    std::cout << "main chain policy tests passed\n";
    return 0;
}

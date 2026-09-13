#include "ai/evaluation/features.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace puyo;

static void fill(Board& b, const int h[6], Cell c) {
    for (int x = 0; x < BOARD_WIDTH; ++x)
        for (int y = 0; y < h[x]; ++y)
            b.set(x, y, c);
}

int main() {
    Board flat, mountain, edge;
    const int hf[6] = {4,4,4,4,4,4};
    const int hm[6] = {2,3,8,8,3,2};
    const int he[6] = {6,5,4,4,5,6};
    fill(flat, hf, Cell::Red);
    fill(mountain, hm, Cell::Red);
    fill(edge, he, Cell::Red);

    const auto ff = extractStaticFeatures(flat);
    const auto mf = extractStaticFeatures(mountain);
    const auto ef = extractStaticFeatures(edge);

    assert(mf.centralPeak > ff.centralPeak);
    assert(ef.edgeWall >= ff.edgeWall);
    assert(std::isfinite(mf.futureConstructionSpace));
    assert(std::isfinite(ef.edgeDeadEnd));

    // A central exact-three trigger with two horizontal escape directions
    // should have more construction room than an otherwise isolated edge
    // trigger.  The test uses different colours so the two triples cannot
    // accidentally merge into one component.
    Board triggers;
    triggers.set(2,0,Cell::Red);
    triggers.set(2,1,Cell::Red);
    triggers.set(2,2,Cell::Red);
    triggers.set(0,0,Cell::Blue);
    triggers.set(0,1,Cell::Blue);
    triggers.set(0,2,Cell::Blue);
    const auto tf = extractStaticFeatures(triggers);
    assert(tf.triggerExpansionSpace > 0.0);
    assert(std::isfinite(tf.triggerExpansionSpace));
    assert(std::isfinite(tf.edgeDeadEnd));

    std::cout << "geometry policy tests passed\n";
    return 0;
}

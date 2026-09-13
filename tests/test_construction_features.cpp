#include "ai/evaluation/features.h"
#include "ai/evaluation/weights.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace puyo;

static void putFlat(Board& b, const int h[6], Cell c = Cell::Red) {
    for (int x = 0; x < BOARD_WIDTH; ++x)
        for (int y = 0; y < h[x]; ++y)
            b.set(x, y, c);
}

int main() {
    // Stable boards cannot contain a real 4/5 group without immediately
    // firing. The feature therefore measures latent 4/5 unit potential.
    Board unit;
    unit.set(1,0,Cell::Red);
    unit.set(2,0,Cell::Red);
    unit.set(3,0,Cell::Red);
    auto f = extractStaticFeatures(unit);
    assert(f.chainUnit4 >= 1.0);
    assert(f.chainUnit5 >= 0.0);

    // A flat surface should be smoother than an alternating mountain/valley.
    Board flat;
    const int fh[6] = {3,3,3,3,3,3};
    putFlat(flat, fh);
    Board rough;
    const int rh[6] = {1,5,1,5,1,5};
    putFlat(rough, rh);
    auto ff = extractStaticFeatures(flat);
    auto rf = extractStaticFeatures(rough);
    assert(ff.surfaceRoughness < rf.surfaceRoughness);
    assert(ff.maxStep < rf.maxStep);
    assert(ff.heightVariance < rf.heightVariance);

    // A legal gravity-stable board has no dead cells beneath its column tops.
    assert(ff.deadSpace == 0.0);

    // Deliberately create a hole and verify it is detected.
    Board hole;
    hole.set(0,0,Cell::Blue);
    hole.set(0,2,Cell::Blue);
    auto hf = extractStaticFeatures(hole);
    assert(hf.deadSpace >= 1.0);

    // Weight API remains finite and exposes all new structural terms.
    Weights w = amaBuildWeights();
    assert(weightCount() == 27);
    for (int i = 15; i <= 24; ++i) {
        assert(weightName(i)[0] != '\0');
        assert(std::isfinite(getWeight(w, i)));
    }

    std::cout << "construction feature tests passed\n";
    return 0;
}

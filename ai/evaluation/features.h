#pragma once

#include "../simulation/board.h"

namespace puyo {

struct Features {
    double chain = 0.0;
    double y = 0.0;
    double key = 0.0;
    double chi = 0.0;

    double shape = 0.0;
    double well = 0.0;
    double bump = 0.0;
    double form = 0.0;
    double chainPotential = 0.0;

    double link2 = 0.0;
    double link3 = 0.0;

    double waste14 = 0.0;
    double side = 0.0;
    double nuisance = 0.0;

    // Human-style construction geometry. These are intentionally separate
    // from the old ama-derived shape terms so they can be tuned independently.
    double chainUnit4 = 0.0;
    double chainUnit5 = 0.0;
    double oversizedUnit = 0.0;
    double surfaceRoughness = 0.0;
    double maxStep = 0.0;
    double deadSpace = 0.0;
    double buildSpace = 0.0;
    double tailSpace = 0.0;
    double heightVariance = 0.0;
    double edgeWall = 0.0;

    double tear = 0.0;
    double waste = 0.0;
};

Features extractStaticFeatures(const Board& board);

} // namespace puyo

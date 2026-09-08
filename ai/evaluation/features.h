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

    double link2 = 0.0;
    double link3 = 0.0;

    double waste14 = 0.0;
    double side = 0.0;
    double nuisance = 0.0;

    double tear = 0.0;
    double waste = 0.0;
};

Features extractStaticFeatures(const Board& board);

} // namespace puyo

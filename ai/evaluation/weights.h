#pragma once

namespace puyo {

struct Weights {
    double chain;
    double y;
    double key;
    double chi;

    double shape;
    double well;
    double bump;
    double form;

    double link2;
    double link3;

    double waste14;
    double side;
    double nuisance;

    double tear;
    double waste;
};

Weights amaBuildWeights();

} // namespace puyo

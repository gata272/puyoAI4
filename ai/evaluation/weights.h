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
    double chainPotential;
    double longChainPotential;

    double link2;
    double link3;

    double waste14;
    double side;
    double nuisance;

    double tear;
    double waste;
};

Weights amaBuildWeights();

/// Stable names used by the browser developer-mode tuner.
const char* weightName(int index);
double getWeight(const Weights& weights, int index);
bool setWeight(Weights& weights, int index, double value);
int weightCount();

} // namespace puyo

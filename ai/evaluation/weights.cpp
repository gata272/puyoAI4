#include "weights.h"
#include <cmath>

namespace puyo {

Weights amaBuildWeights() {
    // Values from citrus610/ama's public config.json "build" profile.
    return {
        1000, // chain
        289,  // y
        -200, // key
        200,  // chi
        -100, // shape
        -100, // well
        -100, // bump
        50,   // form
        1000,  // chainPotential (local extension potential)
        1200, // longChainPotential (latent large-chain construction)
        150,  // link2
        250,  // link3
        -50,  // waste14
        0,    // side
        -250, // nuisance
        -250, // tear
        -250  // waste
    };
}

} // namespace puyo

namespace {
constexpr const char* kWeightNames[] = {
    "chain", "y", "key", "chi", "shape", "well", "bump", "form",
    "chainPotential", "longChainPotential", "link2", "link3",
    "waste14", "side", "nuisance", "tear", "waste"
};
constexpr int kWeightCount = static_cast<int>(sizeof(kWeightNames) / sizeof(kWeightNames[0]));
}
namespace puyo {
const char* weightName(int index) {
    return (index >= 0 && index < kWeightCount) ? kWeightNames[index] : "";
}
double getWeight(const Weights& w, int i) {
    switch(i) {
        case 0: return w.chain; case 1: return w.y; case 2: return w.key; case 3: return w.chi;
        case 4: return w.shape; case 5: return w.well; case 6: return w.bump; case 7: return w.form;
        case 8: return w.chainPotential; case 9: return w.longChainPotential; case 10: return w.link2;
        case 11: return w.link3; case 12: return w.waste14; case 13: return w.side;
        case 14: return w.nuisance; case 15: return w.tear; case 16: return w.waste;
        default: return 0.0;
    }
}
bool setWeight(Weights& w, int i, double v) {
    if (i < 0 || i >= kWeightCount || !std::isfinite(v)) return false;
    switch(i) {
        case 0: w.chain=v; break; case 1: w.y=v; break; case 2: w.key=v; break; case 3: w.chi=v; break;
        case 4: w.shape=v; break; case 5: w.well=v; break; case 6: w.bump=v; break; case 7: w.form=v; break;
        case 8: w.chainPotential=v; break; case 9: w.longChainPotential=v; break; case 10: w.link2=v; break;
        case 11: w.link3=v; break; case 12: w.waste14=v; break; case 13: w.side=v; break;
        case 14: w.nuisance=v; break; case 15: w.tear=v; break; case 16: w.waste=v; break;
    }
    return true;
}
int weightCount() { return kWeightCount; }
} // namespace puyo

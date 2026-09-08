#include "weights.h"

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

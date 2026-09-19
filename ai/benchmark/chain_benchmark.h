#pragma once

#include <string>

namespace puyo {

struct ChainBenchmarkConfig {
    int games = 25;
    int turns = 50;
    int seed = 20260908;
    int depth = 3;
    int beamWidth = 8;
    // Print one progress line per completed game. Enabled for the browser and
    // CLI benchmark so long runs visibly advance instead of appearing stuck.
    bool progress = true;
    // Record a per-turn decision trace in the returned JSON. This is intended
    // for benchmark analysis and is independent of normal-play logging.
    bool recordDecisionLog = true;
};

// Runs a deterministic, single-player benchmark. Every configuration using
// the same seed/games/turns receives exactly the same generated piece corpus.
// The returned string is a JSON object suitable for the browser UI.
std::string runChainBenchmark(const ChainBenchmarkConfig& config);

} // namespace puyo

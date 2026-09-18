#include "ai/benchmark/chain_benchmark.h"

#include <cassert>
#include <string>

int main() {
    puyo::ChainBenchmarkConfig config;
    config.games = 3;
    config.turns = 8;
    config.seed = 12345;
    config.depth = 2;
    config.beamWidth = 4;
    config.progress = false;

    const std::string json = puyo::runChainBenchmark(config);
    assert(json.find("\"version\":2") != std::string::npos);
    assert(json.find("\"games\":3") != std::string::npos);
    assert(json.find("\"turns\":8") != std::string::npos);
    assert(json.find("\"gameOverReasons\":{") != std::string::npos);
    assert(json.find("\"no_safe_move\":") != std::string::npos);
    assert(json.find("\"selected_death_with_safe_move\":") != std::string::npos);
    assert(json.find("\"averageTurns\":") != std::string::npos);
    return 0;
}

#include "ai/benchmark/chain_benchmark.h"

#include <cassert>
#include <string>

int main() {
    puyo::ChainBenchmarkConfig config;
    config.games = 1;
    config.turns = 3;
    config.seed = 12345;
    config.depth = 2;
    config.beamWidth = 4;
    config.progress = false;

    const std::string json = puyo::runChainBenchmark(config);
    assert(json.find("\"version\":4") != std::string::npos);
    assert(json.find("\"games\":1") != std::string::npos);
    assert(json.find("\"turns\":3") != std::string::npos);
    assert(json.find("\"gameOverReasons\":{") != std::string::npos);
    assert(json.find("\"no_safe_move\":") != std::string::npos);
    assert(json.find("\"selected_death_with_safe_move\":") != std::string::npos);
    assert(json.find("\"averageTurns\":") != std::string::npos);
    assert(json.find("\"diagnosticHistory\":5") != std::string::npos);
    assert(json.find("\"averageSafeMovesBeforeDeath\":[") != std::string::npos);
    assert(json.find("\"diagnosticCounts\":[") != std::string::npos);
    assert(json.find("\"recordDecisionLog\":true") != std::string::npos);
    assert(json.find("\"decisionLog\":[") != std::string::npos);
    assert(json.find("\"gameSummaries\":[") != std::string::npos);
    assert(json.find("\"preBoard\":\"") != std::string::npos);
    assert(json.find("\"debugLog\":\"") != std::string::npos);
    return 0;
}

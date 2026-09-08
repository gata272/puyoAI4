#include "../ai/benchmark/chain_benchmark.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    puyo::ChainBenchmarkConfig config;
    if (argc > 1) config.games = std::atoi(argv[1]);
    if (argc > 2) config.turns = std::atoi(argv[2]);
    if (argc > 3) config.seed = std::atoi(argv[3]);
    if (argc > 4) config.depth = std::atoi(argv[4]);
    if (argc > 5) config.beamWidth = std::atoi(argv[5]);

    std::cout << puyo::runChainBenchmark(config) << '\n';
    return 0;
}

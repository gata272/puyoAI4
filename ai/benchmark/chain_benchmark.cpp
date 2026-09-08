#include "chain_benchmark.h"

#include "../ai.h"
#include "../simulation/simulator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <cstdint>
#include <numeric>
#include <vector>

namespace puyo {
namespace {

constexpr int kColors = 4;
constexpr int kMinGames = 1;
constexpr int kMaxGames = 5000;
constexpr int kMinTurns = 1;
constexpr int kMaxTurns = 500;
constexpr int kMinDepth = 1;
constexpr int kMaxDepth = 8;
constexpr int kMinBeam = 1;
constexpr int kMaxBeam = 128;

struct GameStats {
    int maxChain = 0;
    int score = 0;
    int turns = 0;
    bool gameOver = false;
};

int clampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(value, hi));
}

// SplitMix64 gives deterministic, independent seeds without depending on
// implementation-specific std::seed_seq behavior.
std::uint64_t splitMix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

std::vector<PuyoPair> makeQueue(int seed, int game, int turns) {
    const std::uint64_t mixed =
        splitMix64(static_cast<std::uint64_t>(static_cast<std::int64_t>(seed))) ^
        splitMix64(static_cast<std::uint64_t>(game) + 0xD1B54A32D192ED03ULL);

    std::mt19937 rng(static_cast<std::uint32_t>(mixed));
    std::uniform_int_distribution<int> color(1, kColors);

    std::vector<PuyoPair> queue;
    queue.reserve(static_cast<std::size_t>(turns + 6));
    for (int i = 0; i < turns + 6; ++i) {
        queue.push_back({color(rng), color(rng)});
    }
    return queue;
}

double percentile(std::vector<int> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    if (values.size() == 1) return static_cast<double>(values.front());

    const double pos = p * static_cast<double>(values.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(pos));
    const double frac = pos - static_cast<double>(lo);
    return values[lo] * (1.0 - frac) + values[hi] * frac;
}

std::string jsonNumber(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << value;
    return out.str();
}

std::string jsonBool(bool value) {
    return value ? "true" : "false";
}

} // namespace

std::string runChainBenchmark(const ChainBenchmarkConfig& rawConfig) {
    ChainBenchmarkConfig config = rawConfig;
    config.games = clampInt(config.games, kMinGames, kMaxGames);
    config.turns = clampInt(config.turns, kMinTurns, kMaxTurns);
    config.depth = clampInt(config.depth, kMinDepth, kMaxDepth);
    config.beamWidth = clampInt(config.beamWidth, kMinBeam, kMaxBeam);

    std::vector<int> maxChains;
    maxChains.reserve(config.games);

    long long totalScore = 0;
    long long totalTurns = 0;
    long long totalThinkMicros = 0;
    int totalMoves = 0;
    int gamesOver = 0;
    int globalMaxChain = 0;

    const auto benchmarkStart = std::chrono::steady_clock::now();

    for (int game = 0; game < config.games; ++game) {
        AI ai;
        ai.reset();
        Board board;
        const auto queue = makeQueue(config.seed, game, config.turns);

        GameStats stats;

        for (int turn = 0; turn < config.turns; ++turn) {
            // AI needs the current pair plus two lookahead pairs.
            std::vector<PuyoPair> pieces;
            pieces.reserve(6);
            for (int i = 0; i < 6 && turn + i < static_cast<int>(queue.size()); ++i) {
                pieces.push_back(queue[turn + i]);
            }
            if (pieces.empty()) break;

            const auto thinkStart = std::chrono::steady_clock::now();
            const Move move = ai.chooseMove(
                turn,
                board,
                pieces,
                config.depth,
                config.beamWidth
            );
            const auto thinkEnd = std::chrono::steady_clock::now();
            totalThinkMicros += std::chrono::duration_cast<std::chrono::microseconds>(
                thinkEnd - thinkStart
            ).count();
            ++totalMoves;

            if (!move.valid) {
                stats.gameOver = true;
                break;
            }

            const SimulationResult sim = Simulator::drop(board, pieces[0], move);
            if (sim.gameOver && !sim.allClear) {
                stats.gameOver = true;
                break;
            }

            board = sim.board;
            stats.maxChain = std::max(stats.maxChain, sim.chains);
            stats.score += sim.score;
            ++stats.turns;
        }

        if (stats.gameOver) ++gamesOver;
        totalScore += stats.score;
        totalTurns += stats.turns;
        globalMaxChain = std::max(globalMaxChain, stats.maxChain);
        maxChains.push_back(stats.maxChain);
    }

    const auto benchmarkEnd = std::chrono::steady_clock::now();
    const double wallMs = std::chrono::duration<double, std::milli>(
        benchmarkEnd - benchmarkStart
    ).count();

    const double avgMaxChain =
        static_cast<double>(std::accumulate(maxChains.begin(), maxChains.end(), 0LL)) /
        static_cast<double>(maxChains.size());

    auto countAtLeast = [&](int threshold) {
        return static_cast<int>(std::count_if(
            maxChains.begin(),
            maxChains.end(),
            [threshold](int value) { return value >= threshold; }
        ));
    };

    const double avgTurns =
        static_cast<double>(totalTurns) / static_cast<double>(config.games);
    const double avgScore =
        static_cast<double>(totalScore) / static_cast<double>(config.games);
    const double avgThinkMs = totalMoves > 0
        ? static_cast<double>(totalThinkMicros) / static_cast<double>(totalMoves) / 1000.0
        : 0.0;

    std::ostringstream json;
    json << "{";
    json << "\"version\":1,";
    json << "\"games\":" << config.games << ",";
    json << "\"turns\":" << config.turns << ",";
    json << "\"seed\":" << config.seed << ",";
    json << "\"depth\":" << config.depth << ",";
    json << "\"beamWidth\":" << config.beamWidth << ",";
    json << "\"averageMaxChain\":" << jsonNumber(avgMaxChain) << ",";
    json << "\"medianMaxChain\":" << jsonNumber(percentile(maxChains, 0.50)) << ",";
    json << "\"p90MaxChain\":" << jsonNumber(percentile(maxChains, 0.90)) << ",";
    json << "\"maxChain\":" << globalMaxChain << ",";
    json << "\"atLeast5\":" << countAtLeast(5) << ",";
    json << "\"atLeast8\":" << countAtLeast(8) << ",";
    json << "\"atLeast10\":" << countAtLeast(10) << ",";
    json << "\"atLeast12\":" << countAtLeast(12) << ",";
    json << "\"averageScore\":" << jsonNumber(avgScore) << ",";
    json << "\"averageTurns\":" << jsonNumber(avgTurns) << ",";
    json << "\"gamesOver\":" << gamesOver << ",";
    json << "\"averageThinkMs\":" << jsonNumber(avgThinkMs) << ",";
    json << "\"totalWallMs\":" << jsonNumber(wallMs) << ",";
    json << "\"deterministic\":" << jsonBool(true);
    json << "}";

    return json.str();
}

} // namespace puyo

#include "chain_benchmark.h"

#include "../ai.h"
#include "../simulation/simulator.h"
#include "../search/move_generator.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <cstdint>
#include <numeric>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace puyo {
namespace {

constexpr int kColors = 4;
constexpr int kMinGames = 1;
constexpr int kMaxGames = 5000;
constexpr int kMinTurns = 1;
constexpr int kMaxTurns = 500;
constexpr int kMinDepth = 1;
constexpr int kMaxDepth = 50;
constexpr int kMinBeam = 1;
constexpr int kMaxBeam = 500;

enum class GameOverReason {
    None,
    InvalidMove,
    NoGeometricMove,
    NoSafeMove,
    SelectedDeathWithSafeMove,
    Other
};

const char* gameOverReasonName(GameOverReason reason) {
    switch (reason) {
        case GameOverReason::None: return "none";
        case GameOverReason::InvalidMove: return "invalid_move";
        case GameOverReason::NoGeometricMove: return "no_geometric_move";
        case GameOverReason::NoSafeMove: return "no_safe_move";
        case GameOverReason::SelectedDeathWithSafeMove: return "selected_death_with_safe_move";
        case GameOverReason::Other: return "other";
    }
    return "other";
}

struct GameStats {
    int maxChain = 0;
    int score = 0;
    int turns = 0;
    bool gameOver = false;
    GameOverReason gameOverReason = GameOverReason::None;
    int maxHeightAtEnd = 0;
    int dangerColumnHeightAtEnd = 0;
    int occupiedAtEnd = 0;
    int geometricMovesAtEnd = 0;
    int safeMovesAtEnd = 0;
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
    queue.reserve(static_cast<std::size_t>(turns + 10));
    for (int i = 0; i < turns + 10; ++i) {
        queue.push_back({color(rng), color(rng)});
    }
    return queue;
}


void printProgress(const std::string& message) {
#ifdef __EMSCRIPTEN__
    emscripten_log(EM_LOG_CONSOLE, "%s", message.c_str());
#else
    std::cerr << message << std::endl;
#endif
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
    std::array<int, 6> gameOverReasons{};

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
            pieces.reserve(3);
            for (int i = 0; i < 3 && turn + i < static_cast<int>(queue.size()); ++i) {
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
                stats.gameOverReason = GameOverReason::InvalidMove;
                break;
            }

            const auto geometricMoves = generateLegalMoves(board, pieces[0]);
            if (geometricMoves.empty()) {
                stats.gameOver = true;
                stats.gameOverReason = GameOverReason::NoGeometricMove;
                break;
            }

            const SimulationResult sim = Simulator::drop(board, pieces[0], move);
            if (sim.gameOver && !sim.allClear) {
                int safeMoves = 0;
                for (const auto& candidateMove : geometricMoves) {
                    const SimulationResult candidateSim =
                        Simulator::drop(board, pieces[0], candidateMove);
                    if (!candidateSim.gameOver || candidateSim.allClear) {
                        ++safeMoves;
                    }
                }
                stats.safeMovesAtEnd = safeMoves;
                stats.geometricMovesAtEnd = static_cast<int>(geometricMoves.size());
                if (safeMoves == 0) {
                    stats.gameOverReason = GameOverReason::NoSafeMove;
                } else {
                    stats.gameOverReason = GameOverReason::SelectedDeathWithSafeMove;
                }
                stats.gameOver = true;
                board = sim.board;
                break;
            }

            board = sim.board;
            stats.maxChain = std::max(stats.maxChain, sim.chains);
            stats.score += sim.score;
            ++stats.turns;
        }

        const auto endHeights = board.heights();
        stats.maxHeightAtEnd = *std::max_element(endHeights.begin(), endHeights.end());
        stats.dangerColumnHeightAtEnd = endHeights[2];
        stats.occupiedAtEnd = std::accumulate(endHeights.begin(), endHeights.end(), 0);

        if (stats.gameOver) {
            ++gamesOver;
            const int reasonIndex = static_cast<int>(stats.gameOverReason);
            if (reasonIndex >= 0 && reasonIndex < static_cast<int>(gameOverReasons.size())) {
                ++gameOverReasons[static_cast<std::size_t>(reasonIndex)];
            }
        }
        totalScore += stats.score;
        totalTurns += stats.turns;
        globalMaxChain = std::max(globalMaxChain, stats.maxChain);
        maxChains.push_back(stats.maxChain);

        if (config.progress) {
            const int completedGames = game + 1;
            const int completed12 = static_cast<int>(std::count_if(
                maxChains.begin(), maxChains.end(),
                [](int value) { return value >= 12; }));
            const double running12Percent = completedGames > 0
                ? 100.0 * static_cast<double>(completed12) / completedGames
                : 0.0;
            const double runningAvgTurns =
                static_cast<double>(totalTurns) / completedGames;

            std::ostringstream progress;
            progress << "[Benchmark] Game " << completedGames << "/" << config.games
                     << " | max=" << stats.maxChain
                     << " | survived=" << stats.turns << "/" << config.turns
                     << " | 12+=" << completed12 << "/" << completedGames
                     << " (" << std::fixed << std::setprecision(1) << running12Percent << "%)"
                     << " | avgSurvived=" << std::fixed << std::setprecision(1) << runningAvgTurns;
            if (stats.gameOver) {
                progress << " | gameOver=" << gameOverReasonName(stats.gameOverReason)
                         << " | h2=" << stats.dangerColumnHeightAtEnd
                         << " | maxH=" << stats.maxHeightAtEnd
                         << " | occupied=" << stats.occupiedAtEnd;
                if (stats.geometricMovesAtEnd > 0) {
                    progress << " | safeMoves=" << stats.safeMovesAtEnd
                             << "/" << stats.geometricMovesAtEnd;
                }
            }
            printProgress(progress.str());
        }
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
    json << "\"version\":2,";
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
    json << "\"gameOverReasons\":{";
    json << "\"invalid_move\":" << gameOverReasons[static_cast<std::size_t>(GameOverReason::InvalidMove)] << ",";
    json << "\"no_geometric_move\":" << gameOverReasons[static_cast<std::size_t>(GameOverReason::NoGeometricMove)] << ",";
    json << "\"no_safe_move\":" << gameOverReasons[static_cast<std::size_t>(GameOverReason::NoSafeMove)] << ",";
    json << "\"selected_death_with_safe_move\":" << gameOverReasons[static_cast<std::size_t>(GameOverReason::SelectedDeathWithSafeMove)] << ",";
    json << "\"other\":" << gameOverReasons[static_cast<std::size_t>(GameOverReason::Other)];
    json << "},";
    json << "\"averageThinkMs\":" << jsonNumber(avgThinkMs) << ",";
    json << "\"totalWallMs\":" << jsonNumber(wallMs) << ",";
    json << "\"deterministic\":" << jsonBool(true);
    json << "}";

    return json.str();
}

} // namespace puyo

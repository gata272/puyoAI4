#include "chain_benchmark.h"

#include "../ai.h"
#include "../simulation/simulator.h"
#include "../search/move_generator.h"
#include "../evaluation/debug_log.h"

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
constexpr int kDiagnosticsHistory = 5;

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

struct TurnSnapshot {
    int turn = 0;
    Board board;
    PuyoPair piece{};
};

struct SafetySnapshot {
    int turnsBeforeDeath = 0;
    int turn = 0;
    int geometricMoves = 0;
    int safeMoves = 0;
    int maxHeight = 0;
    int dangerColumnHeight = 0;
    int occupied = 0;
};

struct DecisionLogEntry {
    int turn = 0;
    Board preBoard;
    PuyoPair current{};
    PuyoPair next1{};
    PuyoPair next2{};
    bool hasNext1 = false;
    bool hasNext2 = false;
    Move selected{};
    int geometricMoves = 0;
    int safeMoves = 0;
    bool selectedSafe = false;
    SimulationResult simulation{};
    long long thinkMicros = 0;
    std::string debugLog;
};

int countSafeMoves(const Board& board, const PuyoPair& piece, int* geometricCount = nullptr) {
    const auto moves = generateLegalMoves(board, piece);
    if (geometricCount) {
        *geometricCount = static_cast<int>(moves.size());
    }
    int safe = 0;
    for (const auto& move : moves) {
        const SimulationResult result = Simulator::drop(board, piece, move);
        if (!result.gameOver || result.allClear) {
            ++safe;
        }
    }
    return safe;
}

std::vector<SafetySnapshot> diagnoseRecentTurns(
    const std::vector<TurnSnapshot>& history,
    int deathTurn
) {
    std::vector<SafetySnapshot> diagnostics;
    const int start = std::max(0, static_cast<int>(history.size()) - kDiagnosticsHistory);
    for (int i = start; i < static_cast<int>(history.size()); ++i) {
        const TurnSnapshot& snapshot = history[static_cast<std::size_t>(i)];
        int geometric = 0;
        const int safe = countSafeMoves(snapshot.board, snapshot.piece, &geometric);
        const auto heights = snapshot.board.heights();
        diagnostics.push_back({
            deathTurn - snapshot.turn,
            snapshot.turn,
            geometric,
            safe,
            *std::max_element(heights.begin(), heights.end()),
            heights[2],
            std::accumulate(heights.begin(), heights.end(), 0)
        });
    }
    return diagnostics;
}

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


std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c) << std::dec << std::setfill(' ');
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    return out.str();
}

std::string boardCompact(const Board& board) {
    std::string out;
    out.reserve(BOARD_WIDTH * BOARD_HEIGHT);
    for (int y = 0; y < BOARD_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            const int value = static_cast<int>(board.get(x, y));
            out.push_back(static_cast<char>('0' + clampInt(value, 0, 9)));
        }
    }
    return out;
}

void appendPairJson(std::ostringstream& json, const PuyoPair& pair) {
    json << "{\"main\":" << pair.main
         << ",\"sub\":" << pair.sub << "}";
}

void printProgress(const std::string& message) {
#ifdef __EMSCRIPTEN__
    // The benchmark runs inside a dedicated Web Worker.  emscripten_log() is
    // not guaranteed to reach the worker's parent console, so explicitly send
    // progress messages to the worker.  The JS worker forwards them to the UI.
    EM_ASM({
        if (typeof self !== 'undefined' && typeof self.postMessage === 'function') {
            self.postMessage({ type: 'progress', message: UTF8ToString($0) });
        }
    }, message.c_str());
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
    std::array<long long, kDiagnosticsHistory> diagnosticSafeMoveSum{};
    std::array<long long, kDiagnosticsHistory> diagnosticGeometricMoveSum{};
    std::array<int, kDiagnosticsHistory> diagnosticCounts{};
    std::vector<std::vector<DecisionLogEntry>> decisionLogs;
    if (config.recordDecisionLog) decisionLogs.resize(static_cast<std::size_t>(config.games));
    setDebugLogging(config.recordDecisionLog);

    const auto benchmarkStart = std::chrono::steady_clock::now();

    for (int game = 0; game < config.games; ++game) {
        AI ai;
        ai.reset();
        Board board;
        const auto queue = makeQueue(config.seed, game, config.turns);

        GameStats stats;
        std::vector<TurnSnapshot> recentTurns;
        recentTurns.reserve(kDiagnosticsHistory);
        if (config.recordDecisionLog) {
            decisionLogs[static_cast<std::size_t>(game)].reserve(static_cast<std::size_t>(config.turns));
        }

        for (int turn = 0; turn < config.turns; ++turn) {
            // AI needs the current pair plus two lookahead pairs.
            std::vector<PuyoPair> pieces;
            pieces.reserve(3);
            for (int i = 0; i < 3 && turn + i < static_cast<int>(queue.size()); ++i) {
                pieces.push_back(queue[turn + i]);
            }
            if (pieces.empty()) break;

            recentTurns.push_back({turn, board, pieces[0]});
            if (recentTurns.size() > static_cast<std::size_t>(kDiagnosticsHistory)) {
                recentTurns.erase(recentTurns.begin());
            }

            const auto thinkStart = std::chrono::steady_clock::now();
            const Move move = ai.chooseMove(
                turn,
                board,
                pieces,
                config.depth,
                config.beamWidth
            );
            const auto thinkEnd = std::chrono::steady_clock::now();
            const long long thinkMicros = std::chrono::duration_cast<std::chrono::microseconds>(
                thinkEnd - thinkStart
            ).count();
            totalThinkMicros += thinkMicros;
            ++totalMoves;

            DecisionLogEntry logEntry;
            if (config.recordDecisionLog) {
                logEntry.turn = turn;
                logEntry.preBoard = board;
                logEntry.current = pieces[0];
                if (pieces.size() > 1) { logEntry.next1 = pieces[1]; logEntry.hasNext1 = true; }
                if (pieces.size() > 2) { logEntry.next2 = pieces[2]; logEntry.hasNext2 = true; }
                logEntry.selected = move;
                logEntry.thinkMicros = thinkMicros;
                logEntry.debugLog = takeDebugLog();
            } else {
                // Keep the global debug logger clean even when detailed logs are off.
                takeDebugLog();
            }

            if (!move.valid) {
                stats.gameOver = true;
                stats.gameOverReason = GameOverReason::InvalidMove;
                if (config.recordDecisionLog) {
                    logEntry.geometricMoves = 0;
                    logEntry.safeMoves = 0;
                    logEntry.selectedSafe = false;
                    logEntry.simulation = SimulationResult{};
                    decisionLogs[static_cast<std::size_t>(game)].push_back(std::move(logEntry));
                }
                break;
            }

            const auto geometricMoves = generateLegalMoves(board, pieces[0]);
            if (geometricMoves.empty()) {
                stats.gameOver = true;
                stats.gameOverReason = GameOverReason::NoGeometricMove;
                if (config.recordDecisionLog) {
                    logEntry.geometricMoves = 0;
                    logEntry.safeMoves = 0;
                    logEntry.selectedSafe = false;
                    logEntry.simulation = SimulationResult{};
                    decisionLogs[static_cast<std::size_t>(game)].push_back(std::move(logEntry));
                }
                break;
            }

            const SimulationResult sim = Simulator::drop(board, pieces[0], move);
            int safeMoves = 0;
            if (config.recordDecisionLog || (sim.gameOver && !sim.allClear)) {
                for (const auto& candidateMove : geometricMoves) {
                    const SimulationResult candidateSim =
                        Simulator::drop(board, pieces[0], candidateMove);
                    if (!candidateSim.gameOver || candidateSim.allClear) ++safeMoves;
                }
            }

            if (config.recordDecisionLog) {
                logEntry.geometricMoves = static_cast<int>(geometricMoves.size());
                logEntry.safeMoves = safeMoves;
                logEntry.selectedSafe = !sim.gameOver || sim.allClear;
                logEntry.simulation = sim;
                decisionLogs[static_cast<std::size_t>(game)].push_back(std::move(logEntry));
            }

            if (sim.gameOver && !sim.allClear) {
                stats.safeMovesAtEnd = safeMoves;
                stats.geometricMovesAtEnd = static_cast<int>(geometricMoves.size());
                stats.gameOverReason = safeMoves == 0
                    ? GameOverReason::NoSafeMove
                    : GameOverReason::SelectedDeathWithSafeMove;
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

        if (config.progress && stats.gameOver) {
            const auto diagnostics = diagnoseRecentTurns(recentTurns, stats.turns);
            for (const auto& d : diagnostics) {
                if (d.turnsBeforeDeath >= 0 && d.turnsBeforeDeath < kDiagnosticsHistory) {
                    const auto index = static_cast<std::size_t>(d.turnsBeforeDeath);
                    diagnosticSafeMoveSum[index] += d.safeMoves;
                    diagnosticGeometricMoveSum[index] += d.geometricMoves;
                    ++diagnosticCounts[index];
                }
                std::ostringstream detail;
                detail << "[Benchmark]   death-" << d.turnsBeforeDeath
                       << " | turn=" << d.turn
                       << " | safeMoves=" << d.safeMoves << "/" << d.geometricMoves
                       << " | maxH=" << d.maxHeight
                       << " | h2=" << d.dangerColumnHeight
                       << " | occupied=" << d.occupied;
                printProgress(detail.str());
            }
        }

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

    // The benchmark owns the detailed logger. Do not leak benchmark traces into
    // normal-play debug logging after this call returns.
    setDebugLogging(false);

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
    json << "\"version\":4,";
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
    json << "\"diagnosticHistory\":" << kDiagnosticsHistory << ",";
    json << "\"averageSafeMovesBeforeDeath\":[";
    for (int i = 0; i < kDiagnosticsHistory; ++i) {
        if (i > 0) json << ",";
        const double avg = diagnosticCounts[static_cast<std::size_t>(i)] > 0
            ? static_cast<double>(diagnosticSafeMoveSum[static_cast<std::size_t>(i)]) /
              diagnosticCounts[static_cast<std::size_t>(i)]
            : 0.0;
        json << jsonNumber(avg);
    }
    json << "],";
    json << "\"averageGeometricMovesBeforeDeath\":[";
    for (int i = 0; i < kDiagnosticsHistory; ++i) {
        if (i > 0) json << ",";
        const double avg = diagnosticCounts[static_cast<std::size_t>(i)] > 0
            ? static_cast<double>(diagnosticGeometricMoveSum[static_cast<std::size_t>(i)]) /
              diagnosticCounts[static_cast<std::size_t>(i)]
            : 0.0;
        json << jsonNumber(avg);
    }
    json << "],";
    json << "\"diagnosticCounts\":[";
    for (int i = 0; i < kDiagnosticsHistory; ++i) {
        if (i > 0) json << ",";
        json << diagnosticCounts[static_cast<std::size_t>(i)];
    }
    json << "],";
    json << "\"averageThinkMs\":" << jsonNumber(avgThinkMs) << ",";
    json << "\"totalWallMs\":" << jsonNumber(wallMs) << ",";
    json << "\"recordDecisionLog\":" << jsonBool(config.recordDecisionLog) << ",";
    json << "\"gameSummaries\":[";
    if (config.recordDecisionLog) {
        for (std::size_t game = 0; game < decisionLogs.size(); ++game) {
            if (game > 0) json << ",";
            int gameMax = 0;
            int gameScore = 0;
            int loggedTurns = 0;
            bool gameOver = false;
            for (const auto& d : decisionLogs[game]) {
                gameMax = std::max(gameMax, d.simulation.chains);
                gameScore += d.simulation.score;
                ++loggedTurns;
                gameOver = gameOver || (d.simulation.gameOver && !d.simulation.allClear);
            }
            json << "{\"game\":" << game
                 << ",\"maxChain\":" << gameMax
                 << ",\"score\":" << gameScore
                 << ",\"loggedTurns\":" << loggedTurns
                 << ",\"gameOver\":" << jsonBool(gameOver) << "}";
        }
    }
    json << "],";
    json << "\"decisionLog\":";
    if (!config.recordDecisionLog) {
        json << "[]";
    } else {
        json << "[";
        for (std::size_t game = 0; game < decisionLogs.size(); ++game) {
            if (game > 0) json << ",";
            json << "{\"game\":" << game << ",\"turns\":[";
            const auto& logs = decisionLogs[game];
            for (std::size_t i = 0; i < logs.size(); ++i) {
                if (i > 0) json << ",";
                const auto& d = logs[i];
                json << "{\"turn\":" << d.turn
                     << ",\"preBoard\":\"" << boardCompact(d.preBoard) << "\""
                     << ",\"current\":";
                appendPairJson(json, d.current);
                json << ",\"next1\":";
                if (d.hasNext1) appendPairJson(json, d.next1); else json << "null";
                json << ",\"next2\":";
                if (d.hasNext2) appendPairJson(json, d.next2); else json << "null";
                json << ",\"move\":{\"x\":" << d.selected.x
                     << ",\"rotation\":" << d.selected.rotation
                     << ",\"valid\":" << jsonBool(d.selected.valid) << "}"
                     << ",\"geometricMoves\":" << d.geometricMoves
                     << ",\"safeMoves\":" << d.safeMoves
                     << ",\"selectedSafe\":" << jsonBool(d.selectedSafe)
                     << ",\"thinkMicros\":" << d.thinkMicros
                     << ",\"postBoard\":\"" << boardCompact(d.simulation.board) << "\""
                     << ",\"chains\":" << d.simulation.chains
                     << ",\"score\":" << d.simulation.score
                     << ",\"gameOver\":" << jsonBool(d.simulation.gameOver)
                     << ",\"allClear\":" << jsonBool(d.simulation.allClear)
                     << ",\"debugLog\":\"" << jsonEscape(d.debugLog) << "\"}";
            }
            json << "]}";
        }
        json << "]";
    }
    json << ",\"deterministic\":" << jsonBool(true);
    json << "}";

    return json.str();
}

} // namespace puyo

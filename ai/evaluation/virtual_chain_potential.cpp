#include "virtual_chain_potential.h"

#include "../search/move_generator.h"
#include "../simulation/simulator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace puyo {
namespace {

struct ProbeResult {
    int chains = 0;
    int score = 0;
};

// We only need UP and RIGHT. DOWN is the same physical set as UP with the
// ordered colours reversed, and LEFT is the same as RIGHT with the ordered
// colours reversed. Iterating all 16 ordered pairs therefore covers every
// physical two-puyo colour/orientation combination without duplicate work.
std::vector<Move> canonicalMoves(const Board& board, const PuyoPair& pair) {
    std::vector<Move> moves;
    moves.reserve(12);
    for (int rotation : {0, 1}) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (Simulator::findDropY(board, pair, x, rotation) >= 0) {
                moves.push_back({x, rotation, true});
            }
        }
    }
    return moves;
}

} // namespace

VirtualChainFeatures analyzeVirtualChainPotential(const Board& board) {
    VirtualChainFeatures out;

    // Keep the probe out of the very early empty-board phase. Once there is a
    // modest amount of material, it becomes useful; before that, all virtual
    // fires are either zero or dominated by arbitrary first-piece geometry.
    int occupied = 0;
    int maxHeight = 0;
    const auto heights = board.heights();
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        maxHeight = std::max(maxHeight, heights[x]);
        occupied += heights[x];
    }
    if (occupied < 6 || maxHeight < 3) return out;

    std::array<ProbeResult, 192> results{};
    int resultCount = 0;

    for (int a = 1; a <= 4; ++a) {
        for (int b = 1; b <= 4; ++b) {
            const PuyoPair pair{a, b};
            const auto moves = canonicalMoves(board, pair);
            for (const Move& move : moves) {
                const SimulationResult sim = Simulator::drop(board, pair, move);
                if (sim.gameOver && !sim.allClear) continue;
                if (sim.chains <= 0) continue;
                if (resultCount >= static_cast<int>(results.size())) break;
                results[static_cast<std::size_t>(resultCount++)] = {
                    sim.chains, sim.score};
            }
        }
    }

    out.resultCount = resultCount;
    if (resultCount == 0) return out;

    std::sort(results.begin(), results.begin() + resultCount,
        [](const ProbeResult& lhs, const ProbeResult& rhs) {
            if (lhs.chains != rhs.chains) return lhs.chains > rhs.chains;
            return lhs.score > rhs.score;
        });

    const int top = std::min(3, resultCount);
    for (int i = 0; i < top; ++i) {
        out.top3ChainSum += results[static_cast<std::size_t>(i)].chains;
        out.top3ScoreSum += results[static_cast<std::size_t>(i)].score;
    }
    out.bestChain = results[0].chains;
    out.bestScore = results[0].score;
    for (int i = 0; i < resultCount; ++i) {
        if (results[static_cast<std::size_t>(i)].chains >= 2) ++out.count2Plus;
        if (results[static_cast<std::size_t>(i)].chains >= 3) ++out.count3Plus;
    }
    return out;
}

double virtualChainPotentialScore(const VirtualChainFeatures& f, const Board& board) {
    if (f.bestChain <= 0) return 0.0;

    const auto h = board.heights();
    const int maxHeight = *std::max_element(h.begin(), h.end());
    const int occupied = [&]() {
        int n = 0;
        for (int v : h) n += v;
        return n;
    }();

    // These scales are deliberately close to the relative shape of the v13
    // evaluator: best-chain is nonlinear, top-3 prevents one lucky probe from
    // dominating, and counts reward repeatable routes.
    double score = 0.0;
    score += std::pow(static_cast<double>(f.bestChain), 3.0) * 250.0;
    score += static_cast<double>(f.top3ChainSum) * 900.0;
    score += static_cast<double>(std::min(f.count2Plus, 6)) * 450.0;
    score += static_cast<double>(std::min(f.count3Plus, 6)) * 900.0;
    score += static_cast<double>(f.bestScore) * 0.012;
    score += static_cast<double>(f.top3ScoreSum) * 0.003;

    if (f.bestChain >= 8) score += 9000.0;
    if (f.bestChain >= 9) score += 12000.0;
    if (f.bestChain >= 10) score += 30000.0;
    if (f.bestChain >= 11) score += 45000.0;
    if (f.bestChain >= 12) score += 60000.0;
    if (f.bestChain >= 13) score += 80000.0;

    // High stacks are acceptable when there is corresponding virtual
    // firepower. Penalize only the dangerous mismatch, not high boards in
    // general; this is important for 10-13 chain construction.
    if (maxHeight > 10 && f.bestChain < 8) {
        score -= static_cast<double>(maxHeight - 10) * 5000.0;
    }
    if (occupied > 50 && f.bestChain < 9) {
        score -= static_cast<double>(occupied - 50) * 1600.0;
    }
    if (maxHeight >= VISIBLE_HEIGHT) score -= 100000.0;

    return std::clamp(score, -100000.0, 450000.0);
}

} // namespace puyo

#include "beam_search.h"

#include "move_generator.h"
#include "../evaluation/evaluation.h"
#include "../evaluation/trigger_route.h"
#include "../evaluation/long_chain_potential.h"
#include "../evaluation/debug_log.h"
#include "../simulation/simulator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace puyo {
namespace {

// This is a true beam search: every depth expands the current global beam and
// then prunes back to `beamWidth`.  The previous implementation recursively
// expanded a beam independently from every node, which grew roughly as
// width^depth and became impractical once the lookahead was extended.
struct Node {
    Board board;
    Move root;
    double score = 0.0;
    int maxChain = 0;
    int triggerRoute = 0;
    double longPotential = 0.0;
    double structure = 0.0;
    bool gameOver = false;
};

constexpr double kDiscount = 0.85;
constexpr double kChainReward = 15000.0;
constexpr double kDeathPenalty = 250000.0;

// Immediate chain reward is deliberately nonlinear.  It makes an actual
// long chain dominate small scoring differences, while the static evaluator
// remains responsible for constructing the chain before it fires.
double chainReward(int chains) {
    if (chains <= 0) return 0.0;

    // Do not let the search repeatedly cash out 2-3 chains.  Small chains are
    // treated as destructive early firing; the useful reward starts at 5.
    if (chains <= 3) {
        const double c = static_cast<double>(chains);
        return -120000.0 * c * c;
    }
    if (chains == 4) return 25000.0;

    const double c = static_cast<double>(chains);
    return 20000.0 * c * c * c * c;
}

std::vector<Node> expandNode(
    const Node& parent,
    const PuyoPair& pair,
    const std::vector<PuyoPair>& remainingPieces,
    const Weights& weights,
    int nextDepth,
    int maxDepth
) {
    const auto moves = generateLegalMoves(parent.board, pair);
    std::vector<Node> safe;
    std::vector<Node> death;
    safe.reserve(moves.size());
    death.reserve(moves.size());

    for (const Move& move : moves) {
        const SimulationResult sim = Simulator::drop(
            parent.board, pair, move);

        const bool deathMove = sim.gameOver && !sim.allClear;
        EvaluationContext ctx;
        // The trigger planner is deliberately limited to the same three
        // visible pairs a human-style policy is allowed to use.
        const int remaining = std::min(3, static_cast<int>(remainingPieces.size()));
        ctx.lookahead.assign(remainingPieces.begin(), remainingPieces.begin() + remaining);
        // Only terminal candidates pay the expensive ama-style quiet search.
        ctx.quiescenceDepth = (nextDepth >= maxDepth) ? 3 : 0;

        double local = evaluate(sim.board, weights, ctx)
                     + actionPenalty(parent.board, sim, move, weights)
                     + chainReward(sim.chains);
        if (deathMove) local -= kDeathPenalty;

        Node candidate;
        candidate.board = sim.board;
        candidate.root = parent.root.valid ? parent.root : move;
        candidate.score = parent.score + local;
        candidate.maxChain = std::max(parent.maxChain, sim.chains);
        candidate.triggerRoute = std::max(parent.triggerRoute, triggerRouteLength(sim.board));
        candidate.longPotential = longChainPotential(sim.board, ctx.lookahead);
        // Structural guidance is deliberately strongest while the branch is
        // quiet.  Once a real chain has fired, the normal chain objective and
        // simulator state take priority.
        if (sim.chains == 0) {
            // Prepared-group analysis is cheap enough to use during beam
            // expansion.  Full post-trigger tail simulation is deliberately
            // deferred until the final beam so it cannot multiply the search
            // cost at every child.
            candidate.structure += preparedGroupScore(sim.board) * 0.10;
        }
        candidate.gameOver = deathMove;

        if (deathMove) death.push_back(std::move(candidate));
        else safe.push_back(std::move(candidate));
    }

    // Critical fallback rule: death placements are ignored whenever at least
    // one safe placement exists. If none exists, return the least-bad death
    // candidates so the AI can still place the current pair and let the game
    // end naturally instead of producing an invalid/no-op move.
    if (!safe.empty()) return safe;
    return death;
}

bool betterForBeam(const Node& a, const Node& b) {
    if (a.score != b.score) return a.score > b.score;
    return a.maxChain > b.maxChain;
}


void pruneBeam(std::vector<Node>& candidates, int beamWidth) {
    if (static_cast<int>(candidates.size()) <= beamWidth) return;

    // The ordinary evaluator remains the main ranking signal.  In addition,
    // preserve a small route elite so that a quiet A->B->C trigger structure
    // is not discarded merely because it scores less than a short-term shape.
    std::sort(candidates.begin(), candidates.end(), betterForBeam);

    const int routeSlots = std::max(1, beamWidth / 4);
    const int potentialSlots = std::max(1, beamWidth / 4);
    std::vector<Node> selected;
    selected.reserve(static_cast<std::size_t>(beamWidth));

    auto addIfNew = [&](const Node& node) {
        for (const auto& existing : selected) {
            if (existing.root.x == node.root.x &&
                existing.root.rotation == node.root.rotation &&
                existing.triggerRoute == node.triggerRoute &&
                existing.maxChain == node.maxChain &&
                existing.score == node.score) {
                return;
            }
        }
        selected.push_back(node);
    };

    auto routeRank = candidates;
    std::sort(routeRank.begin(), routeRank.end(), [](const Node& a, const Node& b) {
        if (a.triggerRoute != b.triggerRoute) return a.triggerRoute > b.triggerRoute;
        if (a.maxChain != b.maxChain) return a.maxChain > b.maxChain;
        return a.score > b.score;
    });
    for (int i = 0; i < routeSlots && i < static_cast<int>(routeRank.size()); ++i) {
        if (routeRank[static_cast<std::size_t>(i)].triggerRoute >= 2) {
            addIfNew(routeRank[static_cast<std::size_t>(i)]);
        }
    }

    auto potentialRank = candidates;
    std::sort(potentialRank.begin(), potentialRank.end(), [](const Node& a, const Node& b) {
        if (a.longPotential != b.longPotential) return a.longPotential > b.longPotential;
        if (a.triggerRoute != b.triggerRoute) return a.triggerRoute > b.triggerRoute;
        return a.score > b.score;
    });
    for (int i = 0; i < potentialSlots && i < static_cast<int>(potentialRank.size()); ++i) {
        addIfNew(potentialRank[static_cast<std::size_t>(i)]);
    }

    auto structureRank = candidates;
    std::sort(structureRank.begin(), structureRank.end(), [](const Node& a, const Node& b) {
        if (a.structure != b.structure) return a.structure > b.structure;
        if (a.triggerRoute != b.triggerRoute) return a.triggerRoute > b.triggerRoute;
        return a.score > b.score;
    });
    const int structureSlots = std::max(1, beamWidth / 6);
    for (int i = 0; i < structureSlots && i < static_cast<int>(structureRank.size()); ++i) {
        if (structureRank[static_cast<std::size_t>(i)].structure > 0.0) {
            addIfNew(structureRank[static_cast<std::size_t>(i)]);
        }
    }

    for (const auto& node : candidates) {
        if (static_cast<int>(selected.size()) >= beamWidth) break;
        addIfNew(node);
    }

    candidates.swap(selected);
}


std::string debugBoard(const Board& board) {
    std::string out;
    out.reserve(BOARD_WIDTH * (BOARD_HEIGHT + 1));
    for (int y = BOARD_HEIGHT - 1; y >= 0; --y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            const int v = static_cast<int>(board.get(x, y));
            out += (v >= 1 && v <= 4) ? char('0' + v) : (v == 5 ? '#' : '.');
        }
        out += '\n';
    }
    return out;
}

double finalUtility(const Node& n);

void debugBeamSummary(const std::vector<Node>& beam, int depth, int beamWidth) {
    if (!debugLoggingEnabled()) return;
    std::vector<const Node*> ranked;
    ranked.reserve(beam.size());
    for (const auto& n : beam) ranked.push_back(&n);
    std::sort(ranked.begin(), ranked.end(), [](const Node* a, const Node* b) {
        const double ua = finalUtility(*a);
        const double ub = finalUtility(*b);
        if (ua != ub) return ua > ub;
        return a->maxChain > b->maxChain;
    });

    std::ostringstream oss;
    oss << "\n[AI-DEBUG] beam depth=" << depth
        << " size=" << beam.size()
        << " beamWidth=" << beamWidth << '\n';
    const std::size_t n = std::min<std::size_t>(ranked.size(), 8);
    for (std::size_t i = 0; i < n; ++i) {
        const Node& x = *ranked[i];
        oss << "  #" << (i + 1)
            << " root=(" << x.root.x << "," << x.root.rotation << ")"
            << " utility=" << finalUtility(x)
            << " score=" << x.score
            << " maxChain=" << x.maxChain
            << " route=" << x.triggerRoute
            << " longPotential=" << x.longPotential
            << " structure=" << x.structure
            << " gameOver=" << (x.gameOver ? 1 : 0) << '\n';
    }
    debugLog(oss.str());
}

double finalUtility(const Node& n) {
    // Keep actual chain count important, but no longer make it an absolute
    // lexicographic gate. A quiet 5-chain construction with much higher latent
    // potential can now beat a prematurely-cashed 7-chain.
    return n.score + static_cast<double>(n.maxChain) * 70000.0
         + n.longPotential * 5000.0;
}

bool betterFinal(const Node& a, const Node& b) {
    const double ua = finalUtility(a);
    const double ub = finalUtility(b);
    if (ua != ub) {
        // Structure is a tie-breaker, not a replacement for the real chain
        // objective.  This keeps the proven search behavior while preferring
        // the user's trigger-transfer / chain-tail construction when two
        // choices are otherwise close.
        if (std::abs(ua - ub) <= 10000.0 && a.structure != b.structure)
            return a.structure > b.structure;
        return ua > ub;
    }
    if (a.structure != b.structure) return a.structure > b.structure;
    return a.maxChain > b.maxChain;
}

Move chooseRoot(
    const Board& board,
    const std::vector<PuyoPair>& pieces,
    const Weights& weights,
    int maxDepth,
    int beamWidth
) {
    if (pieces.empty()) return {-1, 0, false};

    const int horizon = std::min(
        maxDepth,
        static_cast<int>(pieces.size())
    );
    if (horizon <= 0) return {-1, 0, false};

    // The root is expanded exactly once, then the same beam is propagated
    // globally through subsequent pieces.
    Node root;
    root.board = board;

    std::vector<Node> beam = {root};

    for (int depth = 0; depth < horizon; ++depth) {
        std::vector<Node> next;
        // At most beamWidth * 24 legal placements on a standard 6-column
        // board. Reserve generously without allocating per child later.
        next.reserve(static_cast<std::size_t>(beamWidth) * 24U);

        for (const Node& node : beam) {
            std::vector<PuyoPair> remainingPieces;
            const std::size_t start = static_cast<std::size_t>(depth);
            const std::size_t end = std::min(pieces.size(), start + 3);
            remainingPieces.assign(pieces.begin() + static_cast<std::ptrdiff_t>(start),
                                   pieces.begin() + static_cast<std::ptrdiff_t>(end));
            auto children = expandNode(
                node, pieces[depth], remainingPieces, weights, depth + 1, horizon);
            for (auto& child : children) {
                next.push_back(std::move(child));
            }
        }

        if (next.empty()) return {-1, 0, false};

        pruneBeam(next, beamWidth);

        beam.swap(next);
        debugBeamSummary(beam, depth + 1, beamWidth);

        // Once every surviving branch is a game-over placement, there is no
        // future piece to search. Keep the best one and finish.
        bool allDead = true;
        for (const auto& node : beam) {
            if (!node.gameOver) {
                allDead = false;
                break;
            }
        }
        if (allDead) break;
    }

    // Expensive tail analysis is performed only for the final beam.  This
    // compares the pre-trigger board with the real simulator's post-trigger
    // chain and rewards latent 3+1 / 2+2 tail material without making every
    // beam child pay for a full chain simulation.
    for (auto& node : beam) {
        if (node.maxChain == 0) {
            node.structure += postTriggerTailScore(node.board) * 0.08;
            node.structure -= prematureTriggerRisk(node.board) * 0.05;
        }
    }

    const auto best = std::max_element(
        beam.begin(), beam.end(),
        [](const Node& a, const Node& b) {
            return betterFinal(b, a);
        }
    );

    if (best == beam.end() || !best->root.valid) {
        if (debugLoggingEnabled()) debugLog("[AI-DEBUG] no valid root move");
        return {-1, 0, false};
    }

    if (debugLoggingEnabled()) {
        std::ostringstream oss;
        oss << "[AI-DEBUG] SELECT root=(" << best->root.x << "," << best->root.rotation
            << ") utility=" << finalUtility(*best)
            << " score=" << best->score
            << " maxChain=" << best->maxChain
            << " route=" << best->triggerRoute
            << " longPotential=" << best->longPotential
            << " structure=" << best->structure
            << " gameOver=" << (best->gameOver ? 1 : 0) << "\n"
            << "[AI-DEBUG] selected board (top->bottom):\n"
            << debugBoard(best->board);
        debugLog(oss.str());
    }
    return best->root;
}

} // namespace

Move BeamSearch::chooseMove(
    const Board& board,
    const std::vector<PuyoPair>& pieces,
    const Weights& weights,
    int depth,
    int beamWidth
) const {
    return chooseRoot(board, pieces, weights,
                      std::max(1, depth),
                      std::max(1, beamWidth));
}

} // namespace puyo

#include "beam_search.h"

#include "move_generator.h"
#include "../evaluation/evaluation.h"
#include "../evaluation/trigger_route.h"
#include "../evaluation/long_chain_potential.h"
#include "../evaluation/main_chain.h"
#include "../evaluation/debug_log.h"
#include "../evaluation/virtual_chain_potential.h"
#include "../evaluation/survival_horizon.h"
#include "../simulation/simulator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <array>
#include <numeric>

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
    MainChainPlan mainChain;
    double mainChainScore = 0.0;
    double construction = 0.0;
    double prematureRisk = 0.0;
    double virtualPotential = 0.0;
    VirtualChainFeatures virtualFeatures;
    bool hasVirtual = false;
    TriggerViability triggerViability;
    double triggerViabilityScore = 0.0;
    bool hasTriggerViability = false;
    int futureSafeMoves = -1;
    int previousFutureSafeMoves = -1;
    int previousFutureGeometricMoves = -1;
    int rootFutureSafeMoves = -1;
    double rootSurvivalScore = 0.0;
    bool hasRootSurvival = false;
    int futureGeometricMoves = -1;
    int bestNextGeometricMoves = -1;
    int bestNextSafeMoves = -1;
    double survivalScore = 0.0;
    bool hasSurvival = false;
    bool gameOver = false;
    std::uint64_t boardHash = 0;
    Features features;
    bool hasFeatures = false;
};

constexpr double kDiscount = 0.85;
constexpr double kChainReward = 15000.0;
constexpr double kDeathPenalty = 250000.0;

// Immediate chain reward is deliberately nonlinear.  It makes an actual
// long chain dominate small scoring differences, while the static evaluator
// remains responsible for constructing the chain before it fires.
double chainReward(int chains) {
    // A smooth threshold curve keeps 7-9 from becoming the default cash-out,
    // while leaving enough score headroom for the latent virtual-fire signal
    // to influence construction before the real chain occurs.
    static constexpr double rewards[] = {
        0.0,      // 0
        -18000.0, // 1
        -36000.0, // 2
        -54000.0, // 3
        5000.0,   // 4
        18000.0,  // 5
        45000.0,  // 6
        85000.0,  // 7
        145000.0, // 8
        235000.0, // 9
        360000.0, // 10
        525000.0, // 11
        740000.0, // 12
        1000000.0,// 13
        1300000.0,// 14
        1650000.0 // 15
    };
    if (chains <= 0) return 0.0;
    if (chains < static_cast<int>(std::size(rewards))) return rewards[chains];
    const double c = static_cast<double>(chains);
    return rewards[15] + (c - 15.0) * 400000.0 +
           std::max(0.0, c - 15.0) * std::max(0.0, c - 15.0) * 15000.0;
}

std::uint64_t fastBoardHash(const Board& b) {
    std::uint64_t h = 1469598103934665603ULL;
    for (int y = 0; y < BOARD_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            h ^= static_cast<std::uint64_t>(static_cast<int>(b.get(x, y)) + 1);
            h *= 1099511628211ULL;
        }
    }
    return h;
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
        // `remainingPieces` starts at the current depth.  The current pair has
        // already been placed, so evaluation must see the *next* visible
        // pieces, not the pair that was just consumed.
        const std::size_t lookStart = 1;
        const std::size_t lookEnd = std::min(
            remainingPieces.size(), lookStart + static_cast<std::size_t>(3));
        if (lookStart < lookEnd) {
            ctx.lookahead.assign(
                remainingPieces.begin() + static_cast<std::ptrdiff_t>(lookStart),
                remainingPieces.begin() + static_cast<std::ptrdiff_t>(lookEnd));
        }
        // Only terminal candidates pay the expensive ama-style quiet search.
        ctx.quiescenceDepth = (nextDepth >= maxDepth) ? 3 : 0;

        const Features childFeatures = extractStaticFeatures(sim.board);
        double local = evaluate(sim.board, weights, ctx, &childFeatures)
                     + actionPenalty(parent.board, sim, move, weights,
                                     parent.hasFeatures ? &parent.features : nullptr,
                                     &childFeatures)
                     + chainReward(sim.chains);

        if (deathMove) local -= kDeathPenalty;

        Node candidate;
        candidate.features = childFeatures;
        candidate.hasFeatures = true;
        candidate.board = sim.board;
        candidate.boardHash = fastBoardHash(candidate.board);
        candidate.root = parent.root.valid ? parent.root : move;
        candidate.maxChain = std::max(parent.maxChain, sim.chains);
        // Route/main-chain analysis is intentionally deferred to the terminal
        // beam. Those routines perform hypothetical chain resolutions and are
        // too expensive to run for every child. The fast static evaluator and
        // real chain reward remain on the hot path.
        candidate.triggerRoute = parent.triggerRoute;
        candidate.longPotential = 0.0;
        candidate.mainChainScore = 0.0;
        candidate.construction = 0.0;
        candidate.prematureRisk = 0.0;
        candidate.score = parent.score + local;
        candidate.futureSafeMoves = -1;
        candidate.previousFutureSafeMoves = parent.hasSurvival ? parent.futureSafeMoves : -1;
        candidate.previousFutureGeometricMoves = parent.hasSurvival ? parent.futureGeometricMoves : -1;
        candidate.rootFutureSafeMoves = parent.rootFutureSafeMoves;
        candidate.rootSurvivalScore = parent.rootSurvivalScore;
        candidate.hasRootSurvival = parent.hasRootSurvival;
        candidate.futureGeometricMoves = -1;
        candidate.bestNextGeometricMoves = -1;
        candidate.bestNextSafeMoves = -1;
        candidate.survivalScore = 0.0;
        candidate.hasSurvival = false;
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

double survivalCorrection(const Node& n) {
    if (!n.hasSurvival) return 0.0;
    // Protect prepared long-chain material from being traded away for a small
    // amount of extra mobility. Survival becomes decisive only in the actual
    // collapse zone; a board with strong exact-3/4-unit preparation is allowed
    // to take a calculated risk.
    const double asset = std::clamp(
        n.features.chainUnit4 + 0.5 * n.features.chainUnit5 +
        0.35 * n.features.handoffPotential,
        0.0, 6.0
    );
    const double protection = 1.0 - 0.08 * asset;
    const bool imminent = n.futureSafeMoves >= 0 && n.futureSafeMoves <= 1;
    const bool narrow = n.futureSafeMoves >= 0 && n.futureSafeMoves <= 3;
    const bool collapsing =
        n.futureSafeMoves >= 0 && n.futureSafeMoves <= 3 &&
        n.previousFutureSafeMoves >= 0 &&
        n.previousFutureSafeMoves - n.futureSafeMoves >= 2;
    const bool weakEscape =
        n.futureSafeMoves >= 0 && n.futureSafeMoves <= 3 &&
        n.bestNextSafeMoves >= 0 && n.bestNextSafeMoves <= 1;
    if (!imminent && !narrow && !collapsing && !weakEscape) return 0.0;
    return n.survivalScore * std::clamp(protection, 0.52, 1.0);
}

double beamUtility(const Node& n) {
    return n.score + survivalCorrection(n);
}

bool betterForBeam(const Node& a, const Node& b) {
    const double ua = beamUtility(a);
    const double ub = beamUtility(b);
    if (ua != ub) return ua > ub;
    return a.maxChain > b.maxChain;
}


void pruneBeam(std::vector<Node>& candidates, int beamWidth) {
    if (static_cast<int>(candidates.size()) <= beamWidth) return;

    std::sort(candidates.begin(), candidates.end(), betterForBeam);
    std::vector<Node> selected;
    selected.reserve(static_cast<std::size_t>(beamWidth));

    // First reserve a small number of genuinely safer states when the
    // frontier is entering the danger zone.  This is deliberately bounded:
    // chain-building states still occupy most of the beam, but one heuristic
    // mistake cannot erase every escape route at once.
    bool dangerPresent = false;
    for (const auto& node : candidates) {
        const auto h = node.board.heights();
        if (h[2] >= 8 || *std::max_element(h.begin(), h.end()) >= 10) {
            dangerPresent = true;
            break;
        }
    }

    if (dangerPresent) {
        std::vector<const Node*> safety;
        safety.reserve(candidates.size());
        for (const auto& node : candidates) {
            if (node.hasSurvival) safety.push_back(&node);
        }
        std::sort(safety.begin(), safety.end(), [](const Node* a, const Node* b) {
            const int as = a->futureSafeMoves;
            const int bs = b->futureSafeMoves;
            if (as != bs) return as > bs;
            if (a->bestNextSafeMoves != b->bestNextSafeMoves)
                return a->bestNextSafeMoves > b->bestNextSafeMoves;
            if (a->maxChain != b->maxChain) return a->maxChain > b->maxChain;
            return a->score > b->score;
        });

        // At most a quarter of the beam is a survival reserve.  Prefer
        // chain-preserving survivors when the safety values are equal.
        const int reserve = std::min(
            std::max(1, beamWidth / 4), beamWidth);
        for (const Node* node : safety) {
            if (static_cast<int>(selected.size()) >= reserve) break;
            bool duplicate = false;
            for (const auto& existing : selected) {
                if (existing.boardHash == node->boardHash) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) selected.push_back(*node);
        }
    }

    // Keep a small root-action diversity reserve. This prevents one attractive
    // first move from occupying the entire beam before virtual-fire refinement.
    const int diversitySlots = std::min(6, beamWidth);
    bool seenRoot[BOARD_WIDTH][4]{};
    for (const auto& node : candidates) {
        if (static_cast<int>(selected.size()) >= diversitySlots) break;
        if (node.root.valid && node.root.x >= 0 && node.root.x < BOARD_WIDTH &&
            node.root.rotation >= 0 && node.root.rotation < 4 &&
            !seenRoot[node.root.x][node.root.rotation]) {
            seenRoot[node.root.x][node.root.rotation] = true;
            selected.push_back(node);
        }
    }
    for (const auto& node : candidates) {
        if (static_cast<int>(selected.size()) >= beamWidth) break;
        bool duplicate = false;
        for (const auto& existing : selected) {
            if (existing.root.x == node.root.x &&
                existing.root.rotation == node.root.rotation &&
                existing.score == node.score &&
                existing.maxChain == node.maxChain) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) selected.push_back(node);
    }
    candidates.swap(selected);
}

struct SurvivalCacheKey {
    std::uint64_t board = 0;
    std::uint16_t pair = 0;
    std::uint16_t nextNextPair = 0;
    bool operator==(const SurvivalCacheKey& other) const {
        return board == other.board &&
               pair == other.pair &&
               nextNextPair == other.nextNextPair;
    }
};

struct SurvivalCacheKeyHash {
    std::size_t operator()(const SurvivalCacheKey& key) const {
        std::uint64_t x =
            key.board ^
            (static_cast<std::uint64_t>(key.pair) * 0x9e3779b97f4a7c15ULL) ^
            (static_cast<std::uint64_t>(key.nextNextPair) * 0xbf58476d1ce4e5b9ULL);
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        return static_cast<std::size_t>(x ^ (x >> 31));
    }
};

void applySurvivalProbe(
    std::vector<Node>& candidates,
    const std::vector<PuyoPair>& pieces,
    int depth,
    int probeLimit,
    std::unordered_map<SurvivalCacheKey, SurvivalHorizon, SurvivalCacheKeyHash>& cache
) {
    // At depth d, candidates have consumed pieces[d].  Probe only the next
    // visible pair (and its following geometric mobility) so this remains a
    // human-information horizon rather than hidden-future search.
    if (candidates.empty() || probeLimit <= 0 || depth >= static_cast<int>(pieces.size())) return;
    const PuyoPair* next = &pieces[static_cast<std::size_t>(depth)];
    const PuyoPair* nextNext = (depth + 1 < static_cast<int>(pieces.size()))
        ? &pieces[static_cast<std::size_t>(depth + 1)] : nullptr;

    // Probe the first candidates in their existing deterministic expansion
    // order. Do not sort here: an extra sort of equal-score nodes can change
    // transposition representatives and unintentionally change the AI even
    // when the survival signal is not used for ranking.
    const int n = std::min(probeLimit, static_cast<int>(candidates.size()));
    for (int i = 0; i < n; ++i) {
        Node& node = candidates[static_cast<std::size_t>(i)];
        const auto heights = node.board.heights();
        const int maxHeight = *std::max_element(heights.begin(), heights.end());
        // Start measuring before the literal game-over line.  The dangerous
        // column is column 2, but a tall neighboring stack can make the next
        // horizontal/rotated pair collapse into it.  We therefore begin the
        // probe in the transition zone and let the score remain neutral while
        // mobility is still comfortable.
        if (heights[2] < 8 && maxHeight < 10) continue;
        const int previousSafeMoves = node.previousFutureSafeMoves;
        const int previousGeometricMoves = node.previousFutureGeometricMoves;
        const SurvivalCacheKey key{
            node.boardHash,
            static_cast<std::uint16_t>(
                (static_cast<int>(next->main) << 8) |
                static_cast<int>(next->sub)),
            nextNext
                ? static_cast<std::uint16_t>(
                    (static_cast<int>(nextNext->main) << 8) |
                    static_cast<int>(nextNext->sub))
                : static_cast<std::uint16_t>(0)
        };
        auto it = cache.find(key);
        if (it == cache.end()) {
            it = cache.emplace(key, analyzeSurvivalHorizon(node.board, next, nextNext)).first;
        }
        const SurvivalHorizon& h = it->second;
        node.futureSafeMoves = h.safeMoves;
        node.futureGeometricMoves = h.geometricMoves;
        node.bestNextGeometricMoves = h.bestNextGeometricMoves;
        node.bestNextSafeMoves = h.bestNextSafeMoves;
        node.survivalScore = survivalHorizonScore(h, previousSafeMoves, previousGeometricMoves);
        // Keep the root-level mobility measurement attached to the root action
        // all the way to the final beam. It can then be used for a final safety
        // rescue without changing the intermediate construction search.
        if (depth == 1) {
            node.rootFutureSafeMoves = h.safeMoves;
            node.rootSurvivalScore = node.survivalScore;
            node.hasRootSurvival = true;
        }
        node.hasSurvival = true;
    }
}

void applyVirtualRerank(std::vector<Node>& beam, int topM) {
    if (beam.empty() || topM <= 0) return;
    std::sort(beam.begin(), beam.end(), betterForBeam);
    const int n = std::min(topM, static_cast<int>(beam.size()));
    for (int i = 0; i < n; ++i) {
        Node& node = beam[static_cast<std::size_t>(i)];
        if (!node.hasVirtual) {
            node.virtualFeatures = analyzeVirtualChainPotential(node.board);
            node.virtualPotential = virtualChainPotentialScore(
                node.virtualFeatures, node.board);
            node.hasVirtual = true;
        }
    }
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
            << " virtual=" << x.virtualPotential
            << " vBest=" << x.virtualFeatures.bestChain
            << " vTop3=" << x.virtualFeatures.top3ChainSum
            << " viability=" << x.triggerViabilityScore
            << " vPath=" << x.triggerViability.bestPath
            << " vTrig=" << x.triggerViability.viableTriggers
            << " safeNext=" << x.futureSafeMoves
            << " prevSafe=" << x.previousFutureSafeMoves
            << " geomNext=" << x.futureGeometricMoves
            << " next2Geom=" << x.bestNextGeometricMoves
            << " next2Safe=" << x.bestNextSafeMoves
            << " survival=" << x.survivalScore
            << " rootSafe=" << x.rootFutureSafeMoves
            << " structure=" << x.structure
            << " mainChain=" << x.mainChain.length()
            << " mainContinuity=" << x.mainChainScore
            << " gameOver=" << (x.gameOver ? 1 : 0) << '\n';
    }
    debugLog(oss.str());
}

double finalUtility(const Node& n) {
    // Virtual potential is a test of whether the current construction still
    // has an actual route to a chain.  Route/structure/construction scores are
    // useful only while that viability is intact. Without this gate, the AI
    // can keep rewarding a visually convincing "long-chain shape" after its
    // firing path has already disappeared.
    const double survival = survivalCorrection(n);

    double constructionGate = 1.0;
    if (n.hasVirtual) {
        if (n.virtualPotential < -20000.0) constructionGate = 0.22;
        else if (n.virtualPotential < 0.0) constructionGate = 0.38;
        else if (n.virtualPotential < 20000.0) constructionGate = 0.62;
        else if (n.virtualPotential < 60000.0) constructionGate = 0.84;
    }

    // When virtual firepower is weak, a real trigger-transfer path is the
    // preferred recovery signal. It prevents "safe but short" construction
    // from winning merely because it has a pleasant static shape.
    const double viability = n.hasTriggerViability
        ? n.triggerViabilityScore
        : 0.0;

    const double gatedConstruction =
        (static_cast<double>(n.mainChain.length()) * 16000.0 +
         n.mainChainScore * 0.50 +
         n.construction * 0.06 -
         n.prematureRisk * 0.06) * constructionGate;

    return n.score + static_cast<double>(n.maxChain) * 25000.0
         + n.virtualPotential
         + gatedConstruction
         + viability * (constructionGate < 0.65 ? 0.72 : 0.22)
         + survival;
}

bool betterFinal(const Node& a, const Node& b) {
    const double ua = finalUtility(a);
    const double ub = finalUtility(b);
    if (ua != ub) return ua > ub;
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

    // Very wide beams amplify small heuristic errors on this lightweight
    // evaluator. Keep the user-configured beam value intact for the API, but
    // cap the active construction frontier at 12; this is close to the
    // high-performing v13-style search budget and prevents beam=24/48 from
    // spending most of its work on correlated low-quality states.
    const int activeBeamWidth = std::min(beamWidth, 12);
    if (horizon <= 0) return {-1, 0, false};

    // The root is expanded exactly once, then the same beam is propagated
    // globally through subsequent pieces.
    Node root;
    root.board = board;
    root.boardHash = fastBoardHash(root.board);
    root.features = extractStaticFeatures(board);
    root.hasFeatures = true;
    root.mainChain = analyzeMainChain(board);

    std::vector<Node> beam = {root};
    std::unordered_map<SurvivalCacheKey, SurvivalHorizon, SurvivalCacheKeyHash> survivalCache;
    survivalCache.reserve(static_cast<std::size_t>(activeBeamWidth * 8));

    for (int depth = 0; depth < horizon; ++depth) {
        std::vector<Node> next;
        // At most beamWidth * 24 legal placements on a standard 6-column
        // board. Reserve generously without allocating per child later.
        next.reserve(static_cast<std::size_t>(activeBeamWidth) * 24U);

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

        // Root-layer refinement is especially important: without it a good
        // first move can be discarded before the virtual-fire signal ever
        // sees the board. Probe a moderate prefix here; deeper layers use a
        // smaller top-M budget.
        if (depth == 0) {
            applyVirtualRerank(next, std::min(12, activeBeamWidth));
            // The first move is too important to sample only the top-scoring
            // half of the legal placements.  Probe every root child so a
            // survival-safe chain-preserving move cannot disappear before the
            // final root comparison.
            applySurvivalProbe(next, pieces, depth + 1,
                               static_cast<int>(next.size()), survivalCache);
            std::sort(next.begin(), next.end(), [](const Node& a, const Node& b) {
                return finalUtility(a) > finalUtility(b);
            });
        }

        // Transposition reduction: different move orders can converge to the
        // same board at a given depth. Keep the best-scoring representative.
        // The current depth uses the same future queue for every node, so the
        // board itself is a sufficient state key here. This both removes
        // duplicate work and preserves the strongest root decision.
        std::unordered_map<std::uint64_t, std::size_t> transpositions;
        transpositions.reserve(next.size());
        std::vector<Node> uniqueNext;
        uniqueNext.reserve(next.size());
        for (auto& candidate : next) {
            const auto key = candidate.boardHash;
            const auto it = transpositions.find(key);
            if (it == transpositions.end()) {
                transpositions.emplace(key, uniqueNext.size());
                uniqueNext.push_back(std::move(candidate));
            } else {
                Node& existing = uniqueNext[it->second];
                if (betterForBeam(candidate, existing)) {
                    existing = std::move(candidate);
                }
            }
        }
        next.swap(uniqueNext);

        if (depth > 0) {
            bool dangerPresent = false;
            for (const auto& candidate : next) {
                const auto h = candidate.board.heights();
                const int maxH = *std::max_element(h.begin(), h.end());
                if (h[2] >= 8 || maxH >= 10) {
                    dangerPresent = true;
                    break;
                }
            }
            // In the danger zone, evaluate the whole frontier before pruning.
            // This is a multi-objective beam: chain construction keeps its
            // normal score, while survival gets a chance to reserve an escape
            // route.  On low boards we retain the old cheap top-M probe.
            const int probeLimit = dangerPresent
                ? static_cast<int>(next.size())
                : std::min(12, activeBeamWidth);
            applySurvivalProbe(next, pieces, depth + 1, probeLimit, survivalCache);
        }

        pruneBeam(next, activeBeamWidth);

        beam.swap(next);

        // v13-style mid-search refinement: expensive virtual-fire probes are
        // applied only to the strongest few states, not to every expanded
        // child. At depth 2+ this recovers much of the information value of a
        // full virtual evaluator while keeping the normal beam practical.
        if (depth + 1 >= 2) {
            applyVirtualRerank(beam, std::min(8, activeBeamWidth));
            std::sort(beam.begin(), beam.end(), [](const Node& a, const Node& b) {
                return finalUtility(a) > finalUtility(b);
            });
            if (static_cast<int>(beam.size()) > activeBeamWidth) beam.resize(static_cast<std::size_t>(activeBeamWidth));
        }

        debugBeamSummary(beam, depth + 1, activeBeamWidth);

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

    // Final refinement: evaluate the whole surviving beam with the expensive
    // virtual-fire probe, then apply the user's sequential trigger-transfer
    // analysis only to the strongest virtual candidates.
    applyVirtualRerank(beam, std::min(18, static_cast<int>(beam.size())));
    if (horizon < static_cast<int>(pieces.size())) {
        applySurvivalProbe(beam, pieces, horizon, static_cast<int>(beam.size()), survivalCache);
    }
    std::sort(beam.begin(), beam.end(), [](const Node& a, const Node& b) {
        return finalUtility(a) > finalUtility(b);
    });
    const int structuralM = std::min(6, static_cast<int>(beam.size()));
    for (int i = 0; i < structuralM; ++i) {
        Node& node = beam[static_cast<std::size_t>(i)];
        node.triggerRoute = triggerRouteLength(node.board);
        node.longPotential = longChainPotential(node.board, {});
        node.mainChain = analyzeMainChain(node.board);
        node.construction = mainChainConstructionScore(node.board, node.mainChain);
        node.prematureRisk = prematureMainChainTriggerRisk(node.board, node.mainChain);
        node.structure += postTriggerTailScore(node.board) * 0.05;
        node.structure -= prematureTriggerRisk(node.board) * 0.03;
        node.mainChainScore = mainChainConstructionScore(node.board, node.mainChain) * 0.05;
    }

    // Trigger viability is a final tie-break/recovery signal. Evaluate the
    // same small structural frontier that already pays the expensive route
    // analysis; do not run hypothetical chain resolution for every terminal
    // beam node.
    for (int i = 0; i < structuralM; ++i) {
        Node& node = beam[static_cast<std::size_t>(i)];
        node.triggerViability = analyzeTriggerViability(node.board, node.triggerRoute);
        node.triggerViabilityScore = triggerViabilityScore(node.triggerViability);
        node.hasTriggerViability = true;
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

    // Final safety rescue: do not let a near-immediate mobility collapse win
    // a close final comparison. This is intentionally applied only after the
    // normal chain/structure ranking, so survival cannot globally turn the AI
    // into a safe-but-short builder.
    const Node* selected = &(*best);
    if (best->hasRootSurvival && best->rootFutureSafeMoves <= 1) {
        for (const auto& node : beam) {
            if (!node.root.valid || !node.hasRootSurvival || node.rootFutureSafeMoves < 2) continue;

            // Never trade away an already-found larger chain merely for
            // survival. Rescue is only allowed among equal-max-chain roots.
            if (node.maxChain >= best->maxChain &&
                node.rootFutureSafeMoves > selected->rootFutureSafeMoves) {
                const bool strongEscape =
                    node.rootFutureSafeMoves >= 4 ||
                    (node.rootFutureSafeMoves >= 2 &&
                     node.bestNextSafeMoves >= 2);
                if (strongEscape) selected = &node;
            }
        }
    }

    if (debugLoggingEnabled()) {
        std::ostringstream oss;
        oss << "[AI-DEBUG] SELECT root=(" << selected->root.x << "," << selected->root.rotation
            << ") utility=" << finalUtility(*selected)
            << " score=" << selected->score
            << " maxChain=" << selected->maxChain
            << " route=" << selected->triggerRoute
            << " longPotential=" << selected->longPotential
            << " structure=" << selected->structure
            << " mainChain=" << selected->mainChain.length()
            << " construction=" << selected->construction
            << " prematureRisk=" << selected->prematureRisk
            << " viability=" << selected->triggerViabilityScore
            << " vPath=" << selected->triggerViability.bestPath
            << " mainRoute=";
        for (std::size_t i = 0; i < selected->mainChain.colors.size(); ++i) {
            if (i) oss << "->";
            oss << selected->mainChain.colors[i];
        }
        oss << " mainContinuity=" << selected->mainChainScore
            << " gameOver=" << (selected->gameOver ? 1 : 0) << "\n"
            << "[AI-DEBUG] selected board (top->bottom):\n"
            << debugBoard(selected->board);
        debugLog(oss.str());
    }
    return selected->root;
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
                      std::clamp(depth, 1, 50),
                      std::clamp(beamWidth, 1, 500));
}

} // namespace puyo

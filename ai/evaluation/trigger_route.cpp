#include "trigger_route.h"

#include "../simulation/simulator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <vector>

namespace puyo {
namespace {

struct Pos { int x; int y; };
struct Group { Cell color; std::vector<Pos> cells; };

bool isColor(Cell c) {
    return c != Cell::Empty && c != Cell::Garbage;
}

bool inside(int x, int y) {
    return x >= 0 && x < BOARD_WIDTH && y >= 0 && y < VISIBLE_HEIGHT;
}

std::vector<Group> groupsOf(const Board& board, int wantedSize = 0) {
    std::vector<Group> groups;
    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};

    for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (seen[x][y] || !isColor(board.get(x, y))) continue;

            Group g{board.get(x, y), {}};
            std::queue<Pos> q;
            q.push({x, y});
            seen[x][y] = true;

            while (!q.empty()) {
                const Pos p = q.front();
                q.pop();
                g.cells.push_back(p);

                for (int d = 0; d < 4; ++d) {
                    const int nx = p.x + dx[d];
                    const int ny = p.y + dy[d];
                    if (!inside(nx, ny) || seen[nx][ny]) continue;
                    if (board.get(nx, ny) != g.color) continue;
                    seen[nx][ny] = true;
                    q.push({nx, ny});
                }
            }

            if (wantedSize == 0 || static_cast<int>(g.cells.size()) == wantedSize)
                groups.push_back(std::move(g));
        }
    }
    return groups;
}

void gravity(Board& board) {
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        int writeY = 0;
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            const Cell c = board.get(x, y);
            if (c != Cell::Empty) board.set(x, writeY++, c);
        }
        while (writeY < BOARD_HEIGHT) board.set(x, writeY++, Cell::Empty);
    }
}

// Remove one exact-3 latent trigger and resolve the resulting board with the
// real simulator. This is deliberately a hypothetical construction test: the
// exact-3 group is not itself fired by the game; we ask what would happen if a
// future placement completed/fired it.
int activateGroup(const Board& board, const Group& trigger, Board* after = nullptr) {
    Board test = board;
    for (const Pos& p : trigger.cells) test.set(p.x, p.y, Cell::Empty);

    int score = 0;
    int erased = 0;
    const int chains = Simulator::resolveBoard(test, score, erased);
    if (after) *after = test;
    return chains;
}

// A hypothetical exact-3 trigger is a root of a sequential chain path. The
// path length is the number of actual simulator chain waves produced after
// that trigger is removed. This naturally permits A -> B -> A -> C and does
// not impose the old four-colour limit.
struct RouteStats {
    int chains = 0;
    int branchWaves = 0;
    double unitQuality = 0.0;
    double dependency = 0.0;
};

void removeWave(Board& board, const std::vector<Group>& firing) {
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};
    std::vector<Pos> popped;
    for (const auto& g : firing) {
        for (const Pos& p : g.cells) {
            popped.push_back(p);
            board.set(p.x, p.y, Cell::Empty);
        }
    }

    // Garbage attached to a popped group is removed in the same wave, just
    // like Simulator::resolveBoard().  Keeping this local makes the route
    // evaluator faithful even when a debug/edited board contains garbage.
    std::sort(popped.begin(), popped.end(), [](const Pos& a, const Pos& b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    popped.erase(std::unique(popped.begin(), popped.end(), [](const Pos& a, const Pos& b) {
        return a.x == b.x && a.y == b.y;
    }), popped.end());

    std::vector<Pos> garbage;
    for (const Pos& p : popped) {
        for (int d = 0; d < 4; ++d) {
            const int nx = p.x + dx[d];
            const int ny = p.y + dy[d];
            if (nx < 0 || nx >= BOARD_WIDTH || ny < 0 || ny >= BOARD_HEIGHT) continue;
            if (board.get(nx, ny) == Cell::Garbage) garbage.push_back({nx, ny});
        }
    }
    std::sort(garbage.begin(), garbage.end(), [](const Pos& a, const Pos& b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    garbage.erase(std::unique(garbage.begin(), garbage.end(), [](const Pos& a, const Pos& b) {
        return a.x == b.x && a.y == b.y;
    }), garbage.end());
    for (const Pos& p : garbage) board.set(p.x, p.y, Cell::Empty);

    for (int x = 0; x < BOARD_WIDTH; ++x) {
        int writeY = 0;
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            const Cell c = board.get(x, y);
            if (c != Cell::Empty) board.set(x, writeY++, c);
        }
        while (writeY < BOARD_HEIGHT) board.set(x, writeY++, Cell::Empty);
    }
}

RouteStats evaluateHypotheticalRoute(const Board& board, const Group& trigger) {
    Board test = board;
    for (const Pos& p : trigger.cells) test.set(p.x, p.y, Cell::Empty);

    // The real simulator applies gravity before checking the next wave.  This
    // is essential for trigger-tail patterns where an upper A falls onto an
    // existing AAA after B disappears.
    gravity(test);

    // The trigger itself is the first chain wave.  We then resolve only the
    // material that becomes connected because that wave disappeared.  This is
    // the key distinction from simply counting many unrelated triples.
    RouteStats stats;
    stats.chains = 1;
    stats.unitQuality += trigger.cells.size() == 3 ? 1.0 : 0.0;

    for (int wave = 0; wave < 24; ++wave) {
        const auto gs = groupsOf(test, 0);
        std::vector<Group> firing;
        for (const auto& g : gs) {
            if (g.cells.size() >= 4) firing.push_back(g);
        }
        if (firing.empty()) break;

        ++stats.chains;
        if (firing.size() > 1) ++stats.branchWaves;

        // 4 and 5 are the human-style sweet spot.  Larger groups are still
        // legal and can be necessary, but they are less chain-efficient.
        double waveQuality = 0.0;
        for (const auto& g : firing) {
            const int n = static_cast<int>(g.cells.size());
            if (n == 4) waveQuality += 1.00;
            else if (n == 5) waveQuality += 1.05;
            else if (n == 6) waveQuality += 0.55;
            else waveQuality += 0.20 / static_cast<double>(std::max(1, n - 5));
        }
        stats.unitQuality += waveQuality;

        // A sequential route gets a small compounding bonus.  This makes
        // 10->11 meaningful, while still leaving the actual chain count as
        // the dominant objective.
        stats.dependency += 1.0 + std::min(2.0, waveQuality);
        removeWave(test, firing);
    }
    return stats;
}

int bestHypotheticalPath(const Board& board) {
    struct Candidate {
        const Group* group = nullptr;
        int score = 0;
    };

    const auto triggerGroups = groupsOf(board, 3);
    std::vector<Candidate> candidates;
    candidates.reserve(triggerGroups.size());
    for (const auto& g : triggerGroups) {
        int s = 0;
        for (const Pos& p : g.cells) {
            constexpr int dx[4] = {1,-1,0,0};
            constexpr int dy[4] = {0,0,1,-1};
            for (int d = 0; d < 4; ++d) {
                const int nx = p.x + dx[d];
                const int ny = p.y + dy[d];
                if (nx < 0 || nx >= BOARD_WIDTH || ny < 0 || ny >= VISIBLE_HEIGHT) continue;
                const Cell c = board.get(nx, ny);
                if (isColor(c) && c != g.color) ++s;
            }
        }
        candidates.push_back({&g, s});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });

    constexpr std::size_t kMaxHypotheticalTriggers = 6;
    int best = 0;
    double bestTie = -1.0;
    const std::size_t limit = std::min(kMaxHypotheticalTriggers, candidates.size());
    for (std::size_t i = 0; i < limit; ++i) {
        const RouteStats stats = evaluateHypotheticalRoute(board, *candidates[i].group);
        // Prefer a longer route first; among equal routes prefer 4/5-sized,
        // non-branching units.  This is deliberately not returned directly as
        // a chain count because callers use triggerRouteLength() as a cheap
        // structural ranking signal.
        const double tie = stats.unitQuality * 10.0
                         + stats.dependency * 3.0
                         - static_cast<double>(stats.branchWaves) * 18.0;
        if (stats.chains > best || (stats.chains == best && tie > bestTie)) {
            best = stats.chains;
            bestTie = tie;
        }
    }
    return best;
}

// Count useful latent groups. Exact 3 is the strongest anchor; exact 2 is a
// weaker preparation state.  Groups of 4+ are intentionally not rewarded as
// latent material because they can fire before the route is ready.
double latentGroupScore(const Board& board) {
    double score = 0.0;
    for (const auto& g : groupsOf(board, 0)) {
        if (g.cells.size() == 3) score += 1800.0;
        else if (g.cells.size() == 2) score += 450.0;
    }
    return score;
}

} // namespace

int triggerRouteLength(const Board& board) {
    return bestHypotheticalPath(board);
}

double chainDependencyPathScore(const Board& board) {
    const int path = bestHypotheticalPath(board);
    if (path <= 0) return 0.0;

    // Keep the established path scale. The detailed route analysis above is
    // used for regression/diagnostic quality, while the production score stays
    // comparable to the previous tuned AI.
    const double p = static_cast<double>(path);
    return std::min(180000.0, 9000.0 * p + 2200.0 * p * p);
}

double chainDependencyBranchPenalty(const Board& board) {
    double penalty = 0.0;
    for (const auto& trigger : groupsOf(board, 3)) {
        const int path = activateGroup(board, trigger);
        if (path <= 0) continue;

        Board test = board;
        for (const Pos& p : trigger.cells) test.set(p.x, p.y, Cell::Empty);
        gravity(test);
        int firstWaveGroups = 0;
        for (const auto& g : groupsOf(test, 0)) {
            if (g.cells.size() >= 4) ++firstWaveGroups;
        }
        if (firstWaveGroups > 1) {
            penalty += static_cast<double>(firstWaveGroups - 1) * 2500.0;
        }
    }
    return std::min(30000.0, penalty);
}

double triggerRelayScore(const Board& board) {
    const double path = chainDependencyPathScore(board);
    const double branches = chainDependencyBranchPenalty(board);
    const double latent = latentGroupScore(board);

    // The path is the primary objective. Latent groups help only as a
    // secondary construction signal; parallel same-wave clearing is negative.
    return std::min(220000.0, path + 0.65 * latent - branches);
}

double triggerAnchorValue(const Board& board) {
    // This function is used on every candidate move by actionPenalty(). Keep
    // it cheap: the expensive sequential route is already evaluated once by
    // triggerRelayScore().
    int triples = 0;
    triples = static_cast<int>(groupsOf(board, 3).size());
    return std::min(12000.0, static_cast<double>(triples) * 3000.0);
}

double triggerQueueScore(const Board& board, const std::vector<PuyoPair>& pieces) {
    const auto triggers = groupsOf(board, 3);
    if (triggers.empty() || pieces.empty()) return 0.0;

    std::array<bool, 5> queued{};
    const std::size_t n = std::min<std::size_t>(3, pieces.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (pieces[i].main >= 1 && pieces[i].main <= 4)
            queued[pieces[i].main] = true;
        if (pieces[i].sub >= 1 && pieces[i].sub <= 4)
            queued[pieces[i].sub] = true;
    }

    double score = 0.0;
    for (const auto& trigger : triggers) {
        // A useful exact-3 anchor gets a small queue bonus when any queued
        // colour can plausibly serve as the next dependency material. Do not
        // require a particular colour: refusing an anchor because the desired
        // colour is absent caused the old AI to get stuck.
        for (int c = 1; c <= 4; ++c) {
            if (queued[c] && c != static_cast<int>(trigger.color)) {
                score += 500.0;
                break;
            }
        }
    }
    return std::min(12000.0, score);
}

double triggerRouteScore(const Board& board) {
    return triggerRelayScore(board);
}

double preparedGroupScore(const Board& board) {
    // Hot-path structural score. Do not run a full hypothetical chain for
    // every exact-3 group here; triggerRelayScore() owns that expensive
    // analysis once per board.
    return std::min(60000.0, latentGroupScore(board));
}

double postTriggerTailScore(const Board& board) {
    double best = 0.0;

    for (const auto& trigger : groupsOf(board, 3)) {
        Board after;
        const int chains = activateGroup(board, trigger, &after);
        if (chains <= 0) continue;

        int fireable = 0;
        for (const auto& g : groupsOf(after, 0)) {
            if (g.cells.size() >= 4) ++fireable;
        }

        // Tail value is based primarily on sequential chain depth. The
        // fireable-group count is deliberately weak so this does not turn
        // parallel clearing into the main strategy.
        best = std::max(
            best,
            static_cast<double>(chains) * 10000.0 +
            static_cast<double>(fireable) * 1000.0
        );
    }

    return std::min(100000.0, best);
}

double prematureTriggerRisk(const Board& board) {
    bool hasFiringGroup = false;
    for (const auto& g : groupsOf(board, 0)) {
        if (g.cells.size() >= 4) {
            hasFiringGroup = true;
            break;
        }
    }
    if (!hasFiringGroup) return 0.0;

    const double latent = preparedGroupScore(board);
    const double path = chainDependencyPathScore(board);
    return latent + path > 15000.0
        ? std::min(35000.0, (latent + path) * 0.18)
        : 0.0;
}

} // namespace puyo

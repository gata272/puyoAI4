#include "chain_blueprint.h"

#include "../simulation/simulator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <queue>
#include <vector>

namespace puyo {
namespace {

struct Pos { int x; int y; };
struct Group { Cell color; std::vector<Pos> cells; };

bool isColor(Cell c) {
    return c != Cell::Empty && c != Cell::Garbage;
}

std::vector<Group> groupsOf(const Board& board) {
    std::vector<Group> out;
    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};

    for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (seen[x][y] || !isColor(board.get(x,y))) continue;
            Group g{board.get(x,y), {}};
            std::queue<Pos> q;
            q.push({x,y});
            seen[x][y] = true;
            while (!q.empty()) {
                const Pos p = q.front(); q.pop();
                g.cells.push_back(p);
                for (int d = 0; d < 4; ++d) {
                    const int nx = p.x + dx[d];
                    const int ny = p.y + dy[d];
                    if (nx < 0 || nx >= BOARD_WIDTH || ny < 0 || ny >= VISIBLE_HEIGHT) continue;
                    if (seen[nx][ny] || board.get(nx,ny) != g.color) continue;
                    seen[nx][ny] = true;
                    q.push({nx,ny});
                }
            }
            out.push_back(std::move(g));
        }
    }
    return out;
}

void gravity(Board& board) {
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        int w = 0;
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            const Cell c = board.get(x,y);
            if (c != Cell::Empty) board.set(x,w++,c);
        }
        while (w < BOARD_HEIGHT) board.set(x,w++,Cell::Empty);
    }
}

void eraseGroup(Board& board, const Group& group) {
    for (const Pos& p : group.cells) board.set(p.x,p.y,Cell::Empty);
    gravity(board);
}

std::uint64_t boardHash(const Board& board) {
    std::uint64_t h = 1469598103934665603ULL;
    for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            h ^= static_cast<std::uint64_t>(static_cast<int>(board.get(x,y)) + 1);
            h *= 1099511628211ULL;
        }
    }
    return h;
}

// A group is a useful next event only when it becomes 4+ after removing the
// previous event. Existing 4+ groups are not counted as a dependency edge.
bool hasNewFireableGroup(const std::vector<Group>& before, const Group& after) {
    if (after.cells.size() < 4) return false;
    for (const Group& g : before) {
        if (g.color != after.color || g.cells.size() < 4) continue;
        // If an already-fireable group of this colour exists, avoid calling an
        // unrelated old group a newly-created dependency.  This conservative
        // rule prevents the blueprint from rewarding simultaneous branches.
        return false;
    }
    return true;
}

int pathFrom(const Board& board, int depth, std::array<std::uint64_t, 8>& history, int historySize) {
    if (depth >= 6) return 0;
    const std::uint64_t h = boardHash(board);
    for (int i = 0; i < historySize; ++i) {
        if (history[static_cast<std::size_t>(i)] == h) return 0;
    }
    if (historySize < static_cast<int>(history.size())) {
        history[static_cast<std::size_t>(historySize)] = h;
    }

    const auto before = groupsOf(board);
    struct Candidate { const Group* group; int size; };
    std::array<Candidate, 2> best{};
    int bestCount = 0;

    for (const Group& g : before) {
        if (g.cells.size() < 4) continue;
        // Keep only two best sequential continuations. This is deliberately a
        // tiny bounded beam: it preserves alternatives while avoiding the
        // combinatorial explosion of a full dependency DAG search.
        const int value = 1000 - static_cast<int>(g.cells.size());
        int pos = bestCount;
        if (bestCount < 2) {
            best[static_cast<std::size_t>(bestCount++)] = {&g, value};
        } else {
            if (value <= best[1].size && value <= best[0].size) continue;
            pos = (best[0].size <= best[1].size) ? 0 : 1;
            best[static_cast<std::size_t>(pos)] = {&g, value};
        }
    }

    int result = 0;
    for (int i = 0; i < bestCount; ++i) {
        Board after = board;
        eraseGroup(after, *best[static_cast<std::size_t>(i)].group);
        std::array<std::uint64_t, 8> childHistory = history;
        result = std::max(result, 1 + pathFrom(after, depth + 1, childHistory, historySize + 1));
    }
    return result;
}

// Static dependency: remove one exact-3 anchor, apply gravity, and follow only
// the single strongest newly-created firing event.  This is intentionally a
// path, not a branch search: simultaneous branch explosions never increase
// the score.  A separate group of the same colour is a different event, so
// A -> B -> A is allowed.
int staticDependencyPath(const Board& board, int depth,
                         std::array<std::uint64_t, 8>& history, int historySize) {
    if (depth >= 7) return 0;
    const std::uint64_t h = boardHash(board);
    for (int i = 0; i < historySize; ++i) {
        if (history[static_cast<std::size_t>(i)] == h) return 0;
    }
    if (historySize < static_cast<int>(history.size())) {
        history[static_cast<std::size_t>(historySize)] = h;
    }

    const auto before = groupsOf(board);
    int best = 0;
    int triggerExamined = 0;
    for (const Group& trigger : before) {
        if (trigger.cells.size() != 3) continue;
        if (++triggerExamined > 6) break;

        Board after = board;
        eraseGroup(after, trigger);
        const auto next = groupsOf(after);

        int local = 1; // the anchor itself is one blueprint event
        int nextExamined = 0;
        for (const Group& n : next) {
            if (++nextExamined > 6) break;
            if (!hasNewFireableGroup(before, n)) continue;
            std::array<std::uint64_t, 8> childHistory = history;
            local = std::max(local, 2 + pathFrom(after, depth + 1, childHistory, historySize + 1));
        }
        best = std::max(best, local);
    }
    return best;
}

bool extensionReachable(const Board& board, int x, int y) {
    if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= VISIBLE_HEIGHT) return false;
    if (board.get(x,y) != Cell::Empty) return false;
    const auto h = board.heights();
    return h[x] == y;
}

bool queueHas(const std::vector<PuyoPair>& q, Cell c) {
    const int target = static_cast<int>(c);
    for (std::size_t i = 0; i < std::min<std::size_t>(3,q.size()); ++i) {
        if (q[i].main == target || q[i].sub == target) return true;
    }
    return false;
}

int countAdjacentExtensionCells(const Board& board, const Group& g) {
    bool used[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    int n = 0;
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};
    for (const Pos& p : g.cells) {
        for (int d = 0; d < 4; ++d) {
            const int nx = p.x + dx[d], ny = p.y + dy[d];
            if (!extensionReachable(board,nx,ny) || used[nx][ny]) continue;
            used[nx][ny] = true;
            ++n;
        }
    }
    return n;
}

} // namespace

ChainBlueprint quickChainBlueprint(
    const Board& board,
    const std::vector<PuyoPair>& lookahead
) {
    ChainBlueprint b;
    const auto gs = groupsOf(board);
    for (const Group& g : gs) {
        if (g.cells.size() == 3) ++b.preparedTriples;
        else if (g.cells.size() == 2) ++b.preparedPairs;
    }

    // One-step dependency only.  This is deliberately cheap enough for every
    // beam child; the full recursive blueprint is reserved for terminal beam
    // states.  A separate group of the same colour is still counted as a
    // distinct event, so A -> B -> A remains possible.
    int best = 0;
    for (const Group& trigger : gs) {
        if (trigger.cells.size() != 3) continue;
        Board after = board;
        eraseGroup(after, trigger);
        const auto next = groupsOf(after);
        for (const Group& n : next) {
            if (hasNewFireableGroup(gs, n)) {
                best = std::max(best, 2);
                ++b.completedLinks;
                if (n.color == trigger.color) ++b.reusableColorLinks;
                break;
            }
        }
    }
    b.longestPath = best;
    int latent = 0;
    for (const Group& g : gs) {
        if ((g.cells.size() == 2 || g.cells.size() == 3) &&
            countAdjacentExtensionCells(board, g) > 0) {
            ++latent;
        }
    }
    b.latentLinks = latent;
    for (const Group& g : gs) {
        if (g.cells.size() == 3 && countAdjacentExtensionCells(board,g) == 1) ++b.fragileLinks;
    }
    b.score =
        static_cast<double>(b.longestPath) * 15000.0 +
        static_cast<double>(b.completedLinks) * 1100.0 +
        static_cast<double>(b.latentLinks) * 500.0 +
        static_cast<double>(b.reusableColorLinks) * 1800.0 +
        static_cast<double>(b.preparedTriples) * 700.0 +
        static_cast<double>(b.preparedPairs) * 120.0 -
        static_cast<double>(b.fragileLinks) * 250.0;
    (void)lookahead;
    return b;
}

ChainBlueprint analyzeChainBlueprint(
    const Board& board,
    const std::vector<PuyoPair>& lookahead
) {
    ChainBlueprint b;
    const auto gs = groupsOf(board);

    for (const Group& g : gs) {
        if (g.cells.size() == 3) ++b.preparedTriples;
        else if (g.cells.size() == 2) ++b.preparedPairs;
    }

    // Count completed sequential dependencies without treating same-colour
    // reuse as a cycle: every board group/event is a distinct node.
    std::array<std::uint64_t, 8> history{};
    b.longestPath = staticDependencyPath(board, 0, history, 0);

    // Estimate latent one-step links.  This deliberately follows a single
    // route, not branches, so adding more simultaneous deletions never wins.
    for (const Group& g : gs) {
        if (g.cells.size() != 2 && g.cells.size() != 3) continue;
        const int slots = countAdjacentExtensionCells(board,g);
        if (slots <= 0) continue;
        ++b.latentLinks;
        if (queueHas(lookahead,g.color)) ++b.completedLinks;
        if (g.cells.size() == 3 && slots == 1) ++b.fragileLinks;
    }

    // Same-colour reuse is a feature only when there are multiple distinct
    // prepared groups of that colour. It makes A->B->A possible without
    // pretending that a colour itself is a graph node.
    for (int c = 1; c <= 4; ++c) {
        int count = 0;
        for (const Group& g : gs) {
            if (static_cast<int>(g.color) == c && (g.cells.size() == 2 || g.cells.size() == 3)) ++count;
        }
        if (count >= 2) b.reusableColorLinks += count - 1;
    }

    // Do not reward simultaneous branch explosions.  The path score is based
    // on the longest sequential route; latentLinks are only a small tie-break.
    b.score =
        static_cast<double>(b.longestPath) * 15000.0 +
        static_cast<double>(b.latentLinks) * 650.0 +
        static_cast<double>(b.completedLinks) * 1100.0 +
        static_cast<double>(b.reusableColorLinks) * 1800.0 +
        static_cast<double>(b.preparedTriples) * 900.0 +
        static_cast<double>(b.preparedPairs) * 180.0 -
        static_cast<double>(b.fragileLinks) * 300.0;

    return b;
}

double chainBlueprintScore(
    const Board& board,
    const std::vector<PuyoPair>& lookahead
) {
    return std::min(180000.0, std::max(0.0,
        analyzeChainBlueprint(board, lookahead).score));
}

} // namespace puyo

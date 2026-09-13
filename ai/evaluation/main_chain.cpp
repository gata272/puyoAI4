#include "main_chain.h"

#include <algorithm>
#include <array>
#include <queue>
#include <utility>
#include <vector>

namespace puyo {
namespace {

struct Pos { int x; int y; };
struct Group { Cell color; std::vector<Pos> cells; };

bool isColor(Cell c) {
    return c >= Cell::Red && c <= Cell::Yellow;
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
                    if (nx < 0 || nx >= BOARD_WIDTH ||
                        ny < 0 || ny >= VISIBLE_HEIGHT || seen[nx][ny]) continue;
                    if (board.get(nx,ny) == g.color) {
                        seen[nx][ny] = true;
                        q.push({nx,ny});
                    }
                }
            }
            out.push_back(std::move(g));
        }
    }
    return out;
}

void gravity(Board& board) {
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        int writeY = 0;
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            const Cell c = board.get(x,y);
            if (c != Cell::Empty) board.set(x,writeY++,c);
        }
        while (writeY < BOARD_HEIGHT) board.set(x,writeY++,Cell::Empty);
    }
}

void removeGroups(Board& board, const std::vector<Group>& groups) {
    for (const auto& g : groups) {
        for (const auto& p : g.cells) board.set(p.x,p.y,Cell::Empty);
    }
    // Match the simulator's garbage behavior: garbage adjacent to a popped
    // colour group is removed in the same wave.
    std::vector<Pos> garbage;
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};
    for (const auto& g : groups) {
        for (const auto& p : g.cells) {
            for (int d = 0; d < 4; ++d) {
                const int nx = p.x + dx[d];
                const int ny = p.y + dy[d];
                if (nx < 0 || nx >= BOARD_WIDTH || ny < 0 || ny >= BOARD_HEIGHT) continue;
                if (board.get(nx,ny) == Cell::Garbage) garbage.push_back({nx,ny});
            }
        }
    }
    std::sort(garbage.begin(), garbage.end(), [](const Pos& a, const Pos& b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    garbage.erase(std::unique(garbage.begin(), garbage.end(), [](const Pos& a, const Pos& b) {
        return a.x == b.x && a.y == b.y;
    }), garbage.end());
    for (const auto& p : garbage) board.set(p.x,p.y,Cell::Empty);
    gravity(board);
}

struct Candidate {
    Group trigger;
    MainChainPlan plan;
    int preScore = 0;
};

MainChainPlan routeFromTrigger(const Board& source, const Group& trigger) {
    MainChainPlan plan;
    plan.colors.push_back(static_cast<int>(trigger.color));
    plan.anchorX = trigger.cells.front().x;
    plan.anchorY = trigger.cells.front().y;

    Board board = source;
    removeGroups(board, {trigger});

    // A route is intentionally sequential: all groups in one wave count as
    // one chain step. We keep the largest group colour as the representative
    // for that wave, while recording parallel waves as a branch warning.
    constexpr int kMaxWaves = 24;
    for (int wave = 0; wave < kMaxWaves; ++wave) {
        const auto gs = groupsOf(board);
        std::vector<Group> firing;
        for (const auto& g : gs) {
            if (g.cells.size() >= 4) firing.push_back(g);
        }
        if (firing.empty()) break;

        if (firing.size() > 1) ++plan.branchWaves;

        auto best = std::max_element(firing.begin(), firing.end(),
            [](const Group& a, const Group& b) {
                return a.cells.size() < b.cells.size();
            });
        plan.colors.push_back(static_cast<int>(best->color));
        removeGroups(board, firing);
    }
    return plan;
}

} // namespace

MainChainPlan analyzeMainChain(const Board& board) {
    const auto gs = groupsOf(board);
    std::vector<Candidate> candidates;

    for (const auto& g : gs) {
        // Exact three is the ideal prepared trigger. A 4+ group is already
        // firing and must not become the preferred "construction" anchor.
        if (g.cells.size() != 3) continue;
        int nearby = 0;
        constexpr int dx[4] = {1,-1,0,0};
        constexpr int dy[4] = {0,0,1,-1};
        for (const auto& p : g.cells) {
            for (int d = 0; d < 4; ++d) {
                const int nx = p.x + dx[d];
                const int ny = p.y + dy[d];
                if (nx < 0 || nx >= BOARD_WIDTH || ny < 0 || ny >= VISIBLE_HEIGHT) continue;
                const Cell c = board.get(nx,ny);
                if (isColor(c) && c != g.color) ++nearby;
            }
        }
        Candidate c;
        c.trigger = g;
        c.plan = routeFromTrigger(board, g);
        c.preScore = c.plan.length() * 100 + nearby * 3 - c.plan.branchWaves * 12;
        candidates.push_back(std::move(c));
    }

    if (candidates.empty()) return {};

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.plan.length() != b.plan.length()) return a.plan.length() > b.plan.length();
        if (a.plan.branchWaves != b.plan.branchWaves) return a.plan.branchWaves < b.plan.branchWaves;
        return a.preScore > b.preScore;
    });
    return candidates.front().plan;
}

double mainChainContinuityScore(
    const MainChainPlan& parent,
    const MainChainPlan& child,
    int actualChains
) {
    if (child.empty()) {
        return parent.empty() ? 0.0 : -18000.0;
    }
    if (parent.empty()) {
        return static_cast<double>(child.length()) * 5500.0
             - static_cast<double>(child.branchWaves) * 2500.0;
    }

    // If a real chain fired, advance the remembered route by the number of
    // waves that actually occurred. This prevents a legitimate B -> A -> C
    // chain from being treated as "abandoning" B after B has just fired.
    const std::size_t offset = std::min<std::size_t>(
        static_cast<std::size_t>(std::max(0, actualChains)), parent.colors.size());
    const std::size_t remainingParent = parent.colors.size() - offset;
    const std::size_t common = std::min(remainingParent, child.colors.size());
    std::size_t prefix = 0;
    while (prefix < common &&
           parent.colors[offset + prefix] == child.colors[prefix]) {
        ++prefix;
    }

    double score = static_cast<double>(prefix) * 9000.0;
    if (static_cast<std::size_t>(child.length()) > remainingParent && prefix == remainingParent) {
        score += static_cast<double>(child.length() - remainingParent) * 24000.0;
    } else if (prefix < remainingParent) {
        const double lost = static_cast<double>(remainingParent - prefix);
        score -= lost * (actualChains > 0 ? 5000.0 : 14000.0);
    }

    if (child.branchWaves > 0) {
        score -= static_cast<double>(child.branchWaves) * 3500.0;
    }
    return score;
}

double mainChainCleanupScore(
    const MainChainPlan& parent,
    const MainChainPlan& child,
    int actualChains
) {
    if (actualChains > 0 || parent.empty() || child.empty()) return 0.0;

    const std::size_t common = std::min(parent.colors.size(), child.colors.size());
    std::size_t prefix = 0;
    while (prefix < common && parent.colors[prefix] == child.colors[prefix]) ++prefix;
    if (prefix < 2) return 0.0;

    // Cleanup is deliberately secondary. It can help when the next useful
    // colour is absent, but it must never outrank a real extension.
    return 4500.0 + static_cast<double>(prefix) * 700.0;
}

} // namespace puyo

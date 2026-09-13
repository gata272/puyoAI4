#include "main_chain.h"

#include <algorithm>
#include <array>
#include <cmath>
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
    // The BFS first cell is arbitrary.  Use the centroid of the trigger
    // instead, so spatial policy does not accidentally depend on traversal
    // order.
    int sumX = 0;
    int sumY = 0;
    for (const Pos& p : trigger.cells) {
        sumX += p.x;
        sumY += p.y;
    }
    plan.anchorX = static_cast<int>(
        std::lround(static_cast<double>(sumX) / trigger.cells.size()));
    plan.anchorY = static_cast<int>(
        std::lround(static_cast<double>(sumY) / trigger.cells.size()));

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


namespace {

bool inside(int x, int y) { return x >= 0 && x < BOARD_WIDTH && y >= 0 && y < VISIBLE_HEIGHT; }

bool routeContainsColor(const MainChainPlan& plan, Cell c) {
    const int value = static_cast<int>(c);
    return std::find(plan.colors.begin(), plan.colors.end(), value) != plan.colors.end();
}

int countColorGroups(const Board& board, int wantedSize, Cell wantedColor = Cell::Empty) {
    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    int count = 0;
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};

    for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            const Cell c = board.get(x,y);
            if (!isColor(c) || seen[x][y] ||
                (wantedColor != Cell::Empty && c != wantedColor)) continue;

            int size = 0;
            std::queue<Pos> q;
            q.push({x,y});
            seen[x][y] = true;
            while (!q.empty()) {
                const Pos p = q.front(); q.pop();
                ++size;
                for (int d = 0; d < 4; ++d) {
                    const int nx = p.x + dx[d];
                    const int ny = p.y + dy[d];
                    if (!inside(nx,ny) || seen[nx][ny]) continue;
                    if (board.get(nx,ny) == c) {
                        seen[nx][ny] = true;
                        q.push({nx,ny});
                    }
                }
            }
            if (size == wantedSize) ++count;
        }
    }
    return count;
}

int countEmptyCells(const Board& board) {
    int empty = 0;
    for (int x = 0; x < BOARD_WIDTH; ++x)
        for (int y = 0; y < VISIBLE_HEIGHT; ++y)
            if (board.get(x,y) == Cell::Empty) ++empty;
    return empty;
}

double edgeWallQuality(const Board& board) {
    const auto h = board.heights();
    const double left = static_cast<double>(h[0] + h[1]);
    const double right = static_cast<double>(h[4] + h[5]);
    const double center = static_cast<double>(h[2] + h[3]);

    // A useful wall is high enough to contain material, but the center must
    // remain lower.  Do not reward a uniformly high board.
    const double wall = std::max(0.0, std::min(left, right) - center * 0.5);
    const double asymmetry = std::abs(left - right);
    return std::max(0.0, wall - 0.20 * asymmetry);
}

double routeSpatialQuality(const Board& board, const MainChainPlan& plan) {
    if (plan.empty()) return 0.0;

    const auto h = board.heights();
    // The anchor is only a representative cell, so use its column plus the
    // actual route length to prefer an expandable middle region.
    const int x = std::clamp(plan.anchorX, 0, BOARD_WIDTH - 1);
    const double centerDistance = std::abs(static_cast<double>(x) - 2.5);

    // A route can live near an edge if it has a wall beside it; otherwise
    // central columns are safer because both horizontal directions remain.
    double score = 0.0;
    if (x == 0 || x == 5) score -= 500.0;
    else score += 500.0;
    score -= centerDistance * 180.0;

    const int leftSpace = VISIBLE_HEIGHT - h[0];
    const int rightSpace = VISIBLE_HEIGHT - h[5];
    const int verticalSpace = VISIBLE_HEIGHT - h[x];
    score += std::min(8, verticalSpace) * 120.0;
    score += std::min(6, std::min(leftSpace, rightSpace)) * 70.0;
    return score;
}

} // namespace

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

double mainChainConstructionScore(
    const Board& board,
    const MainChainPlan& plan
) {
    if (plan.empty()) return 0.0;

    const int triples = countColorGroups(board, 3);
    const int pairs = countColorGroups(board, 2);
    const int empty = countEmptyCells(board);

    // A long sequential route is worth much more than a pile of unrelated
    // pairs. Repeated colours are intentionally not penalized.
    const double route = static_cast<double>(plan.length());
    double score = route * 4200.0;
    if (plan.length() >= 2) score += static_cast<double>(plan.length() - 1) * 2600.0;
    score += std::min(triples, 5) * 500.0;
    score += std::min(pairs, 8) * 90.0;

    // Keep enough empty cells for the next dependency transfer. The score
    // saturates so "empty board" does not beat a real prepared structure.
    score += std::min(empty, 28) * 35.0;

    // Central/side geometry and edge walls are deliberately soft.
    score += routeSpatialQuality(board, plan);
    score += edgeWallQuality(board) * 45.0;

    // If the route's anchor colour is absent from the plan this cannot happen
    // for a valid analyzed plan, but retaining the guard makes the metric safe
    // for externally constructed test plans.
    if (plan.anchorX >= 0 && plan.anchorY >= 0 &&
        routeContainsColor(plan, board.get(plan.anchorX, plan.anchorY))) {
        score += 250.0;
    }

    return std::clamp(score, -20000.0, 90000.0);
}

double prematureMainChainTriggerRisk(
    const Board& board,
    const MainChainPlan& plan
) {
    if (plan.empty()) return 0.0;

    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    double risk = 0.0;
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};

    for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            const Cell c = board.get(x,y);
            if (!isColor(c) || seen[x][y]) continue;

            int size = 0;
            bool hasEmptyAttachment = false;
            std::queue<Pos> q;
            q.push({x,y});
            seen[x][y] = true;
            while (!q.empty()) {
                const Pos p = q.front(); q.pop();
                ++size;
                for (int d = 0; d < 4; ++d) {
                    const int nx = p.x + dx[d];
                    const int ny = p.y + dy[d];
                    if (!inside(nx,ny)) continue;
                    const Cell n = board.get(nx,ny);
                    if (n == Cell::Empty) hasEmptyAttachment = true;
                    else if (n == c && !seen[nx][ny]) {
                        seen[nx][ny] = true;
                        q.push({nx,ny});
                    }
                }
            }

            if (size != 3 || !hasEmptyAttachment) continue;

            // A prepared triple whose colour belongs to the current route is
            // useful, not premature. Unrelated triples are only a soft risk.
            if (!routeContainsColor(plan, c)) {
                risk += 1200.0;
            }
        }
    }

    return std::min(18000.0, risk);
}

} // namespace puyo

#include "trigger_route.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <queue>
#include <vector>

namespace puyo {
namespace {

struct CellPos { int x; int y; };
struct TriggerGroup {
    Cell color = Cell::Empty;
    std::vector<CellPos> cells;
};

bool isColor(Cell c) {
    return c != Cell::Empty && c != Cell::Garbage;
}

std::vector<CellPos> component(const Board& board, int sx, int sy) {
    const Cell c = board.get(sx, sy);
    std::vector<CellPos> out;
    if (!isColor(c)) return out;

    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    std::queue<CellPos> q;
    q.push({sx, sy});
    seen[sx][sy] = true;

    while (!q.empty()) {
        const CellPos p = q.front();
        q.pop();
        out.push_back(p);

        constexpr int dx[] = {1, -1, 0, 0};
        constexpr int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
            const int nx = p.x + dx[d];
            const int ny = p.y + dy[d];
            if (nx < 0 || nx >= BOARD_WIDTH ||
                ny < 0 || ny >= VISIBLE_HEIGHT || seen[nx][ny]) continue;
            if (board.get(nx, ny) == c) {
                seen[nx][ny] = true;
                q.push({nx, ny});
            }
        }
    }
    return out;
}

std::vector<TriggerGroup> findTriggerGroups(const Board& board) {
    std::vector<TriggerGroup> groups;
    bool visited[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (visited[x][y] || !isColor(board.get(x, y))) continue;
            auto cells = component(board, x, y);
            for (const auto& p : cells) visited[p.x][p.y] = true;
            if (cells.size() == 3) groups.push_back({board.get(x, y), std::move(cells)});
        }
    }
    return groups;
}

std::vector<CellPos> triggerCells(const Board& board, const TriggerGroup& group) {
    std::vector<CellPos> out;
    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    for (const auto& p : group.cells) {
        constexpr int dx[] = {1, -1, 0, 0};
        constexpr int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
            const int x = p.x + dx[d];
            const int y = p.y + dy[d];
            if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= VISIBLE_HEIGHT ||
                seen[x][y] || board.get(x, y) != Cell::Empty) continue;
            seen[x][y] = true;
            out.push_back({x, y});
        }
    }
    return out;
}

bool samePos(const CellPos& a, const CellPos& b) {
    return a.x == b.x && a.y == b.y;
}

// Test one concrete A -> B transfer.  A is a 3-puyo trigger group.  We put an
// A at its trigger cell and a B adjacent to it (the four legal pair
// orientations), remove A, apply gravity, and verify that the inserted B is
// now part of the selected B trigger group.
bool canTransferToGroup(
    const Board& board,
    const TriggerGroup& a,
    const CellPos& aTrigger,
    const TriggerGroup& b
) {
    constexpr int dx[] = {0, -1, 0, 1};
    constexpr int dy[] = {1, 0, -1, 0};

    for (int r = 0; r < 4; ++r) {
        const CellPos bPos{aTrigger.x + dx[r], aTrigger.y + dy[r]};
        if (bPos.x < 0 || bPos.x >= BOARD_WIDTH ||
            bPos.y < 0 || bPos.y >= BOARD_HEIGHT) continue;
        if (board.get(bPos.x, bPos.y) != Cell::Empty) continue;

        bool overlapsA = false;
        for (const auto& p : a.cells) {
            if (samePos(p, bPos)) { overlapsA = true; break; }
        }
        if (overlapsA) continue;

        Board next = board;
        for (const auto& p : a.cells) next.set(p.x, p.y, Cell::Empty);
        next.set(aTrigger.x, aTrigger.y, Cell::Empty);
        next.set(bPos.x, bPos.y, b.color);

        // Compute the destination of the inserted B before gravity.
        int bFinalY = 0;
        for (int y = 0; y < bPos.y; ++y) {
            if (next.get(bPos.x, y) != Cell::Empty) ++bFinalY;
        }

        // Map every original B cell to its post-gravity position.  This lets
        // us distinguish the intended target 3-group from an unrelated group
        // of the same color elsewhere on the board.
        std::vector<CellPos> targetAfter;
        targetAfter.reserve(b.cells.size());
        for (const auto& p : b.cells) {
            int fy = 0;
            for (int y = 0; y < p.y; ++y) {
                if (next.get(p.x, y) != Cell::Empty) ++fy;
            }
            targetAfter.push_back({p.x, fy});
        }

        for (int x = 0; x < BOARD_WIDTH; ++x) {
            int writeY = 0;
            std::array<Cell, BOARD_HEIGHT> column{};
            for (int y = 0; y < BOARD_HEIGHT; ++y) {
                const Cell c = next.get(x, y);
                if (c != Cell::Empty) column[writeY++] = c;
            }
            for (int y = 0; y < BOARD_HEIGHT; ++y) next.set(x, y, column[y]);
        }

        if (bFinalY < 0 || bFinalY >= VISIBLE_HEIGHT ||
            next.get(bPos.x, bFinalY) != b.color) continue;

        const auto insertedComponent = component(next, bPos.x, bFinalY);
        if (insertedComponent.size() < 4) continue;

        int originalBInComponent = 0;
        for (const auto& p : targetAfter) {
            for (const auto& q : insertedComponent) {
                if (samePos(p, q)) {
                    ++originalBInComponent;
                    break;
                }
            }
        }
        if (originalBInComponent >= 3) return true;
    }
    return false;
}

std::vector<std::vector<int>> buildGraph(
    const Board& board,
    const std::vector<TriggerGroup>& groups
) {
    std::vector<std::vector<int>> edge(groups.size());
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const auto triggers = triggerCells(board, groups[i]);
        for (std::size_t j = 0; j < groups.size(); ++j) {
            if (i == j || groups[i].color == groups[j].color) continue;
            bool found = false;
            for (const auto& t : triggers) {
                if (canTransferToGroup(board, groups[i], t, groups[j])) {
                    found = true;
                    break;
                }
            }
            if (found) edge[i].push_back(static_cast<int>(j));
        }
    }
    return edge;
}

int longestPath(const std::vector<std::vector<int>>& edge) {
    const int n = static_cast<int>(edge.size());
    int best = 0;

    // Boards normally contain only a handful of 3-groups.  DFS over simple
    // paths is therefore cheap, while allowing the same color to occur again
    // as long as it is a different trigger group.  A 12-step cap keeps the
    // feature bounded and directly targets the user's 10+ chain objective.
    std::function<void(int, std::uint64_t, int)> dfs =
        [&](int cur, std::uint64_t mask, int len) {
            best = std::max(best, len);
            if (len >= 12 || n > 63) return;
            for (const int next : edge[cur]) {
                const std::uint64_t bit = 1ULL << next;
                if (mask & bit) continue;
                dfs(next, mask | bit, len + 1);
            }
        };

    if (n <= 63) {
        for (int i = 0; i < n; ++i) dfs(i, 1ULL << i, 1);
    }
    return best;
}

} // namespace

int triggerRouteLength(const Board& board) {
    return longestPath(buildGraph(board, findTriggerGroups(board)));
}

double triggerRouteScore(const Board& board) {
    const int length = triggerRouteLength(board);
    if (length <= 1) return 0.0;
    return static_cast<double>(length * length * length) * 500.0;
}

} // namespace puyo

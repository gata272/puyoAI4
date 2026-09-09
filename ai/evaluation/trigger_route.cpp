#include "trigger_route.h"

#include "../simulation/simulator.h"

#include <algorithm>
#include <array>
#include <queue>
#include <vector>

namespace puyo {
namespace {

struct Pos { int x; int y; };

bool color(Cell c) {
    return c != Cell::Empty && c != Cell::Garbage;
}

bool inside(int x, int y) {
    return x >= 0 && x < BOARD_WIDTH && y >= 0 && y < VISIBLE_HEIGHT;
}

std::vector<Pos> component(const Board& board, int sx, int sy) {
    std::vector<Pos> out;
    const Cell c = board.get(sx, sy);
    if (!color(c)) return out;

    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    std::queue<Pos> q;
    q.push({sx, sy});
    seen[sx][sy] = true;

    while (!q.empty()) {
        const Pos p = q.front();
        q.pop();
        out.push_back(p);

        constexpr int dx[] = {1, -1, 0, 0};
        constexpr int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
            const int nx = p.x + dx[d];
            const int ny = p.y + dy[d];
            if (!inside(nx, ny) || seen[nx][ny]) continue;
            if (board.get(nx, ny) == c) {
                seen[nx][ny] = true;
                q.push({nx, ny});
            }
        }
    }
    return out;
}

bool contains(const std::vector<Pos>& cells, int x, int y) {
    return std::any_of(cells.begin(), cells.end(), [&](const Pos& p) {
        return p.x == x && p.y == y;
    });
}

bool hasFourOrMore(const Board& board) {
    bool visited[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (visited[x][y] || !color(board.get(x, y))) continue;
            const auto cells = component(board, x, y);
            for (const auto& p : cells) visited[p.x][p.y] = true;
            if (cells.size() >= 4) return true;
        }
    }
    return false;
}

void placeWithoutResolve(Board& board, const PuyoPair& pair, const Move& move,
                         Pos& mainPos, Pos& subPos) {
    const int y = Simulator::findDropY(board, pair, move.x, move.rotation);
    mainPos = {move.x, y};
    switch (move.rotation & 3) {
        case 0: subPos = {move.x, y + 1}; break;
        case 1: subPos = {move.x - 1, y}; break;
        case 2: subPos = {move.x, y - 1}; break;
        default: subPos = {move.x + 1, y}; break;
    }
    board.set(mainPos.x, mainPos.y, static_cast<Cell>(pair.main));
    board.set(subPos.x, subPos.y, static_cast<Cell>(pair.sub));
}

// Count existing B puyos connected to a prospective B cell, excluding the
// prospective cell itself.  A size of 2 is ideal: the newly inserted B makes
// a 3-group, and one later B drop can finish it.  Size 3 is intentionally
// rejected because it would fire immediately when the relay is constructed.
int adjacentSupport(const Board& board, Pos prospective, Cell b) {
    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    std::queue<Pos> q;
    int count = 0;

    constexpr int dx[] = {1, -1, 0, 0};
    constexpr int dy[] = {0, 0, 1, -1};
    for (int d = 0; d < 4; ++d) {
        const int nx = prospective.x + dx[d];
        const int ny = prospective.y + dy[d];
        if (!inside(nx, ny) || seen[nx][ny] || board.get(nx, ny) != b) continue;
        seen[nx][ny] = true;
        q.push({nx, ny});
    }

    while (!q.empty()) {
        const Pos p = q.front();
        q.pop();
        ++count;
        for (int d = 0; d < 4; ++d) {
            const int nx = p.x + dx[d];
            const int ny = p.y + dy[d];
            if (!inside(nx, ny) || seen[nx][ny] || board.get(nx, ny) != b) continue;
            seen[nx][ny] = true;
            q.push({nx, ny});
        }
    }
    return count;
}

// Return the best number of future B puyos required to make the relay B fire.
// 0 means the B is already a 3-group after inserting the B from B->A, so one
// additional B is required. Larger values mean a weaker but still viable
// latent trigger. We cap the useful range at 3 future puyos.
int relayCost(int support) {
    if (support < 0 || support >= 3) return 99;
    return std::max(1, 3 - support);
}

struct RelayCandidate {
    int cost = 99;
    int x = -1;
    int y = -1;
    int support = 0;
};

std::vector<RelayCandidate> findRelayCandidates(const Board& board) {
    std::vector<RelayCandidate> out;

    // The construction is specifically B below A, so use the vertical
    // rotation with B as main and A as sub.  Enumerate all actual legal drops;
    // this makes the motif consistent with the simulator's collision/gravity
    // rules instead of assuming a cell can be filled arbitrarily.
    for (int a = 1; a <= 4; ++a) {
        for (int b = 1; b <= 4; ++b) {
            if (a == b) continue;
            const PuyoPair pair{b, a};
            for (int x = 0; x < BOARD_WIDTH; ++x) {
                Move move{x, 0, true};
                const int y = Simulator::findDropY(board, pair, x, 0);
                if (y < 0 || y + 1 >= VISIBLE_HEIGHT) continue;

                Board placed = board;
                Pos mainPos{}, subPos{};
                placeWithoutResolve(placed, pair, move, mainPos, subPos);
                if (subPos.y >= VISIBLE_HEIGHT) continue;

                // The relay must not fire while it is being constructed.
                if (hasFourOrMore(placed)) continue;

                // The upper A must be separated from the existing A trigger by
                // exactly one B.  Find an exact-3 A component immediately
                // below that B.
                if (placed.get(mainPos.x, mainPos.y) != static_cast<Cell>(b) ||
                    placed.get(subPos.x, subPos.y) != static_cast<Cell>(a)) {
                    continue;
                }
                if (mainPos.x != subPos.x || subPos.y != mainPos.y + 1) continue;

                bool foundA = false;
                for (int ay = 0; ay < mainPos.y; ++ay) {
                    const int yBelow = mainPos.y - 1;
                    if (ay != yBelow) continue;
                    if (placed.get(mainPos.x, yBelow) != static_cast<Cell>(a)) continue;
                    const auto ag = component(placed, mainPos.x, yBelow);
                    if (ag.size() != 3 || contains(ag, subPos.x, subPos.y)) continue;
                    foundA = true;
                }
                if (!foundA) continue;

                const int support = adjacentSupport(placed, mainPos, static_cast<Cell>(b));
                const int cost = relayCost(support);
                if (cost >= 99) continue;

                out.push_back({cost, mainPos.x, mainPos.y, support});
            }
        }
    }
    return out;
}

} // namespace

int triggerRouteLength(const Board& board) {
    // A route step is now a concrete latent relay, not merely adjacency of
    // two existing 3-groups.  This is intentionally conservative: one board
    // state can expose several relay opportunities, but we count only the
    // strongest one here.  The beam search separately preserves relay-rich
    // states.
    const auto candidates = findRelayCandidates(board);
    if (candidates.empty()) return 0;
    const int bestCost = std::min_element(
        candidates.begin(), candidates.end(),
        [](const RelayCandidate& a, const RelayCandidate& b) {
            return a.cost < b.cost;
        })->cost;
    return 4 - bestCost; // 3 = one B away, 2 = two B away, 1 = three B away.
}

double triggerRelayScore(const Board& board) {
    const auto candidates = findRelayCandidates(board);
    double score = 0.0;
    for (const auto& c : candidates) {
        // Strongly prefer a relay that needs only one future B.  Multiple
        // independent relays are useful because the search can later choose
        // whichever one matches the incoming queue.
        const double strength = (c.cost == 1 ? 12000.0 :
                                 c.cost == 2 ? 4500.0 : 1000.0);
        score += strength;
    }
    return std::min(score, 30000.0);
}

double triggerRouteScore(const Board& board) {
    return triggerRelayScore(board);
}

} // namespace puyo

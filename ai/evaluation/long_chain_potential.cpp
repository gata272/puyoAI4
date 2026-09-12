#include "long_chain_potential.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <queue>
#include <vector>

namespace puyo {
namespace {

bool isColor(Cell c) {
    return c != Cell::Empty && c != Cell::Garbage;
}

struct Group {
    Cell color = Cell::Empty;
    std::vector<std::pair<int,int>> cells;
};

std::vector<Group> groups(const Board& board) {
    std::vector<Group> out;
    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};

    for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (seen[x][y] || !isColor(board.get(x,y))) continue;

            Group g;
            g.color = board.get(x,y);
            std::queue<std::pair<int,int>> q;
            q.push({x,y});
            seen[x][y] = true;

            while (!q.empty()) {
                auto [cx,cy] = q.front();
                q.pop();
                g.cells.push_back({cx,cy});

                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];
                    if (nx < 0 || nx >= BOARD_WIDTH ||
                        ny < 0 || ny >= VISIBLE_HEIGHT || seen[nx][ny]) {
                        continue;
                    }
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

// Only cells immediately reachable by dropping a single vertical puyo are
// counted. This avoids rewarding arbitrary empty cells that cannot actually
// extend a group in one move.
int reachableExtensionCells(const Board& board, const Group& g) {
    bool counted[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    const auto h = board.heights();
    int count = 0;
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};

    for (auto [x,y] : g.cells) {
        for (int d = 0; d < 4; ++d) {
            const int nx = x + dx[d];
            const int ny = y + dy[d];
            if (nx < 0 || nx >= BOARD_WIDTH ||
                ny < 0 || ny >= VISIBLE_HEIGHT) continue;
            if (board.get(nx,ny) != Cell::Empty || h[nx] != ny) continue;
            if (!counted[nx][ny]) {
                counted[nx][ny] = true;
                ++count;
            }
        }
    }
    return count;
}

double shapePotential(const std::array<int,BOARD_WIDTH>& h) {
    double score = 0.0;
    const int maxH = *std::max_element(h.begin(), h.end());

    if (maxH <= 8) score += 2.0;
    else if (maxH <= 11) score += 1.0;
    else if (maxH >= 13) score -= 8.0;

    const double leftSlope = static_cast<double>(h[1] - h[0]);
    const double midSlope = static_cast<double>(h[2] - h[1]);
    const double rightSlope = static_cast<double>(h[5] - h[4]);
    const double rightMidSlope = static_cast<double>(h[4] - h[3]);

    if (leftSlope > 0) score += std::min(leftSlope, 3.0);
    if (rightSlope < 0) score += std::min(-rightSlope, 3.0);
    if (midSlope < 0) score += std::min(-midSlope, 2.0);
    if (rightMidSlope > 0) score += std::min(rightMidSlope, 2.0);

    for (int x = 1; x < 5; ++x) {
        if (h[x] + 3 < std::min(h[x-1], h[x+1])) score -= 2.0;
    }
    return score;
}

double groupPotential(const Board& board, const Group& g) {
    const int n = static_cast<int>(g.cells.size());
    const int slots = reachableExtensionCells(board, g);

    if (n >= 4) return -1.0;
    if (n == 3) return 12.0 + std::min(slots, 4) * 2.5;
    if (n == 2) return 4.0 + std::min(slots, 4) * 1.0;
    return 0.1;
}

// Prefer one strong construction corridor rather than many independent
// fragments. The latter used to make the AI spread its material and lose the
// sequential chain path the new policy is explicitly trying to build.
double bestLatentCorridor(const Board& board) {
    double best = 0.0;
    for (const auto& g : groups(board)) {
        if (g.cells.size() != 2 && g.cells.size() != 3) continue;
        const int slots = reachableExtensionCells(board, g);
        const double sizeValue = g.cells.size() == 3 ? 10.0 : 4.0;
        best = std::max(best, sizeValue + std::min(slots, 4) * 2.0);
    }
    return best;
}

double queueCompatibility(const Board& board, const std::vector<PuyoPair>& lookahead) {
    if (lookahead.empty()) return 0.0;

    bool queued[5]{};
    for (std::size_t i = 0; i < std::min<std::size_t>(3, lookahead.size()); ++i) {
        if (lookahead[i].main >= 1 && lookahead[i].main <= 4)
            queued[lookahead[i].main] = true;
        if (lookahead[i].sub >= 1 && lookahead[i].sub <= 4)
            queued[lookahead[i].sub] = true;
    }

    // Only the best single latent corridor receives the queue bonus. This is
    // intentionally not a "more colours is better" term.
    double best = 0.0;
    for (const auto& g : groups(board)) {
        if (g.cells.size() != 2 && g.cells.size() != 3) continue;
        if (!queued[static_cast<int>(g.color)]) continue;
        const double value = g.cells.size() == 3 ? 4.0 : 2.0;
        best = std::max(best, value);
    }
    return best;
}

} // namespace

double longChainPotential(const Board& board, const std::vector<PuyoPair>& lookahead) {
    const auto gs = groups(board);
    if (gs.empty()) return 0.0;

    double score = 0.0;
    int totalColorCells = 0;

    for (const auto& g : gs) {
        totalColorCells += static_cast<int>(g.cells.size());
        score += groupPotential(board, g);
    }

    // A single strong latent corridor is preferred to a collection of
    // unrelated triples/pairs. The actual sequential path is evaluated by
    // trigger_route.cpp and combined separately by evaluate().
    score += bestLatentCorridor(board) * 2.5;
    score += queueCompatibility(board, lookahead) * 2.0;
    score += 2.0 * shapePotential(board.heights());

    const auto h = board.heights();
    const int maxH = *std::max_element(h.begin(), h.end());
    const int occupied = std::accumulate(h.begin(), h.end(), 0);
    if (maxH >= 11) score -= 5.0 + (maxH - 10) * 2.0;
    if (occupied >= 58) score -= 5.0;

    score += std::min(totalColorCells, 36) * 0.05;
    return std::clamp(score, 0.0, 100.0);
}

} // namespace puyo

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

    for (int y=0; y<VISIBLE_HEIGHT; ++y) {
        for (int x=0; x<BOARD_WIDTH; ++x) {
            if (seen[x][y] || !isColor(board.get(x,y))) continue;
            Group g;
            g.color = board.get(x,y);
            std::queue<std::pair<int,int>> q;
            q.push({x,y});
            seen[x][y] = true;
            while (!q.empty()) {
                auto [cx,cy] = q.front(); q.pop();
                g.cells.push_back({cx,cy});
                for (int d=0; d<4; ++d) {
                    int nx=cx+dx[d], ny=cy+dy[d];
                    if (nx<0 || nx>=BOARD_WIDTH || ny<0 || ny>=VISIBLE_HEIGHT || seen[nx][ny]) continue;
                    if (board.get(nx,ny)==g.color) {
                        seen[nx][ny]=true;
                        q.push({nx,ny});
                    }
                }
            }
            out.push_back(std::move(g));
        }
    }
    return out;
}

// Only the top cell of a column can be occupied by the next falling puyo.
// Counting arbitrary empty cells would systematically overestimate potential.
int reachableExtensionCells(const Board& board, const Group& g) {
    bool counted[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    const auto h = board.heights();
    int count = 0;
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};

    for (auto [x,y] : g.cells) {
        for (int d=0; d<4; ++d) {
            const int nx=x+dx[d], ny=y+dy[d];
            if (nx<0 || nx>=BOARD_WIDTH || ny<0 || ny>=VISIBLE_HEIGHT) continue;
            if (board.get(nx,ny) != Cell::Empty || h[nx] != ny) continue;
            if (!counted[nx][ny]) {
                counted[nx][ny] = true;
                ++count;
            }
        }
    }
    return count;
}

// A column profile that favors the broad S-shaped construction described in
// human large-chain guides, while avoiding a hard-coded single template.
double shapePotential(const std::array<int,BOARD_WIDTH>& h) {
    const double leftSlope = static_cast<double>(h[1]-h[0]);
    const double midSlope = static_cast<double>(h[2]-h[1]);
    const double rightSlope = static_cast<double>(h[5]-h[4]);
    const double rightMidSlope = static_cast<double>(h[4]-h[3]);

    double score = 0.0;
    // Moderate height is useful; near-top fields are dangerous and should not
    // be mistaken for long-chain potential.
    const int maxH = *std::max_element(h.begin(), h.end());
    if (maxH <= 8) score += 2.0;
    else if (maxH <= 10) score += 1.0;
    else if (maxH >= 12) score -= 8.0;

    // Reward one broad turn on each side rather than a perfectly flat field.
    if (leftSlope > 0) score += std::min(leftSlope, 3.0);
    if (rightSlope < 0) score += std::min(-rightSlope, 3.0);
    if (midSlope < 0) score += std::min(-midSlope, 2.0);
    if (rightMidSlope > 0) score += std::min(rightMidSlope, 2.0);

    // Deep isolated wells are hard to extend; shallow wells can be useful.
    for (int x=1; x<5; ++x) {
        if (h[x] + 3 < std::min(h[x-1], h[x+1])) score -= 2.0;
    }
    return score;
}

double groupPotential(const Board& board, const Group& g) {
    const int n = static_cast<int>(g.cells.size());
    const int slots = reachableExtensionCells(board,g);

    if (n >= 4) {
        // A 4+ group is already fireable. It is useful, but for a build AI it
        // is less valuable than an un-fired 2/3 group because it can disappear
        // before the rest of the chain is constructed.
        return 1.0;
    }
    if (n == 3) {
        return 8.0 + std::min(slots,4) * 2.0;
    }
    if (n == 2) {
        return 3.0 + std::min(slots,4) * 0.75;
    }
    return 0.25;
}

double queueCompatibility(const Board& board, const std::vector<PuyoPair>& lookahead) {
    if (lookahead.empty()) return 0.0;
    bool queued[5]{};
    for (std::size_t i=0; i<std::min<std::size_t>(3,lookahead.size()); ++i) {
        if (lookahead[i].main >= 1 && lookahead[i].main <= 4) queued[lookahead[i].main]=true;
        if (lookahead[i].sub >= 1 && lookahead[i].sub <= 4) queued[lookahead[i].sub]=true;
    }

    bool useful[5]{};
    for (const auto& g : groups(board)) {
        const int c=static_cast<int>(g.color);
        if (g.cells.size()==2 || g.cells.size()==3) useful[c]=true;
    }

    double score=0.0;
    for (int c=1;c<=4;++c) {
        if (useful[c] && queued[c]) score += 2.5;
    }
    return score;
}

} // namespace

double longChainPotential(const Board& board, const std::vector<PuyoPair>& lookahead) {
    const auto gs = groups(board);
    if (gs.empty()) return 0.0;

    double score = 0.0;
    int latentTriggers = 0;
    int extendableGroups = 0;
    int totalColorCells = 0;

    for (const auto& g : gs) {
        totalColorCells += static_cast<int>(g.cells.size());
        score += groupPotential(board,g);
        if (g.cells.size() == 2 || g.cells.size() == 3) {
            ++extendableGroups;
            if (reachableExtensionCells(board,g) > 0) ++latentTriggers;
        }
    }

    // Prefer several independent extension opportunities over one giant
    // cluster. This is a diversity term: it keeps the beam from collapsing
    // onto one 7-8-chain corridor.
    score += std::min(extendableGroups,8) * 1.5;
    score += std::min(latentTriggers,8) * 2.0;

    // S-shaped construction potential complements the existing ama-form and
    // trigger evaluators. Keeping those computations out of this hot path is
    // important because this function is called for every beam child.
    score += 2.0 * shapePotential(board.heights());

    score += queueCompatibility(board,lookahead);

    // A crowded field cannot realize a long chain reliably. Penalize height
    // pressure and reward having enough empty cells for later extension.
    const auto h=board.heights();
    const int maxH=*std::max_element(h.begin(),h.end());
    const int occupied=std::accumulate(h.begin(),h.end(),0);
    if (maxH >= 11) score -= 5.0 + (maxH-10)*2.0;
    if (occupied >= 58) score -= 5.0;

    // Mild density reward prevents a sparse board with many isolated pairs
    // from looking better than an actually connected build.
    score += std::min(totalColorCells,36) * 0.05;

    return std::clamp(score, 0.0, 100.0);
}

} // namespace puyo

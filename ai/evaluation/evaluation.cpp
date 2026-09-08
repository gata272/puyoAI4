#include "evaluation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

namespace puyo {
namespace {

struct QuietResult {
    int chainCount = 0;
    int x = 0;
    int key = 0;
    Board remain;
};

bool color(Cell c) {
    return c != Cell::Empty && c != Cell::Garbage;
}

int componentSize(const Board& board, int sx, int sy) {
    const Cell c = board.get(sx, sy);
    if (!color(c)) return 0;
    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    std::queue<std::pair<int,int>> q;
    q.push({sx,sy});
    seen[sx][sy] = true;
    int n = 0;
    while (!q.empty()) {
        auto [x,y] = q.front(); q.pop(); ++n;
        constexpr int dx[4] = {1,-1,0,0};
        constexpr int dy[4] = {0,0,1,-1};
        for (int d=0; d<4; ++d) {
            int nx=x+dx[d], ny=y+dy[d];
            if (nx<0 || nx>=BOARD_WIDTH || ny<0 || ny>=VISIBLE_HEIGHT || seen[nx][ny]) continue;
            if (board.get(nx,ny)==c) { seen[nx][ny]=true; q.push({nx,ny}); }
        }
    }
    return n;
}

bool hasTrigger(const Board& board, int x, int y0) {
    const Cell c = board.get(x,y0);
    if (!color(c)) return false;
    for (int y=0; y<VISIBLE_HEIGHT; ++y) {
        if (board.get(x,y)==c && componentSize(board,x,y)>=4) return true;
    }
    return false;
}

int chiValue(const std::array<int, BOARD_WIDTH>& heights, int x) {
    int chi = 0;
    if (x < 5) {
        for (int i=x+1; i<6; ++i) {
            if (heights[i] > heights[x]) break;
            ++chi;
        }
        for (int i=x+1; i<6; ++i) {
            if (heights[i] >= heights[x]) break;
            ++chi;
        }
    }
    if (x > 0) {
        for (int i=x-1; i>=0; --i) {
            if (heights[i] > heights[x]) break;
            ++chi;
        }
        for (int i=x-1; i>=0; --i) {
            if (heights[i] >= heights[x]) break;
            ++chi;
        }
    }
    return chi;
}

// Direct port of ama's quiet::generate/search idea.  `drop` is the maximum
// number of same-colour single puyos to add at one column before a trigger.
std::vector<QuietResult> quietSearch(const Board& board, int drop) {
    std::vector<QuietResult> out;
    const auto h = board.heights();
    int xmin=2, xmax=2;
    for (int x=3; x<6; ++x) {
        if (h[x] > 11) break;
        ++xmax;
    }
    for (int x=1; x>=0; --x) {
        if (h[x] > 11) break;
        --xmin;
    }

    for (int x=xmin; x<=xmax; ++x) {
        if (x<0 || x>=6) continue;
        const int maxDrop = std::min(drop, 12-h[x]);
        if (maxDrop<=0) continue;
        for (int c=1; c<=4; ++c) {
            Board plan = board;
            for (int n=1; n<=maxDrop; ++n) {
                plan.set(x, h[x]+n-1, static_cast<Cell>(c));
                if (hasTrigger(plan, x, h[x]+n-1)) {
                    QuietResult r;
                    r.chainCount = 1;
                    r.x=x;
                    r.key=n;
                    r.remain=plan;
                    out.push_back(r);
                    break;
                }
            }
        }
    }
    return out;
}

double quietScore(const Board& board, const Weights& w, int drop) {
    double best = -std::numeric_limits<double>::infinity();
    const auto h = board.heights();
    for (const auto& q : quietSearch(board, drop)) {
        const auto f = extractStaticFeatures(q.remain);
        const int chi = chiValue(h, q.x);
        const double score =
            q.chainCount * w.chain +
            h[q.x] * w.y +
            q.key * w.key +
            chi * w.chi +
            f.link2 * w.link2 +
            f.link3 * w.link3;
        best = std::max(best, score);
    }
    return std::isfinite(best) ? best : 0.0;
}

} // namespace

double evaluate(
    const Board& board,
    const Weights& weights,
    const EvaluationContext& context
) {
    const Features f = extractStaticFeatures(board);
    double score =
        f.form * weights.form +
        f.shape * weights.shape +
        f.well * weights.well +
        f.bump * weights.bump +
        f.link2 * weights.link2 +
        f.link3 * weights.link3 +
        f.waste14 * weights.waste14 +
        f.side * weights.side +
        f.nuisance * weights.nuisance;

    // ama's beam evaluator always runs quiet search with a tactical drop
    // depth of 3.  Keep the parameter configurable for benchmarking/tuning.
    if (context.quiescenceDepth > 0) {
        score += quietScore(board, weights, context.quiescenceDepth);
    }
    return score;
}

double actionPenalty(
    const Board& before,
    const SimulationResult& result,
    const Move& move,
    const Weights& weights
) {
    const Features a = extractStaticFeatures(before);
    const Features b = extractStaticFeatures(result.board);
    const double tear = std::max(0.0, (a.link2 + a.link3) - (b.link2 + b.link3));
    // ama uses the number of popped puyos as its waste action feature.
    const double waste = static_cast<double>(result.erased);
    const double movement = std::abs(move.x - 2) + std::min(move.rotation, 4 - move.rotation);
    return (tear + 0.25 * movement) * weights.tear + waste * weights.waste;
}

} // namespace puyo

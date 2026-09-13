#include "features.h"
#include "forms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace puyo {

namespace {

bool isColor(Cell c) {
    return c != Cell::Empty && c != Cell::Garbage;
}

bool same(const Board& board, int x, int y, int nx, int ny) {
    if (nx < 0 || nx >= BOARD_WIDTH || ny < 0 || ny >= VISIBLE_HEIGHT) return false;
    Cell c = board.get(x, y);
    return isColor(c) && board.get(nx, ny) == c;
}

// Port of ama's get_link_23 semantics without SIMD.  l3 marks cells that
// participate in a 3-connected junction/straight triple.  l2 counts the
// remaining directed two-cell connections after expanding the l3 mask to
// neighboring cells, matching FieldBit::get_expand's purpose.
void getLink23(const Board& board, int& link2, int& link3) {
    bool l3[BOARD_WIDTH][VISIBLE_HEIGHT]{};

    for (int x = 0; x < BOARD_WIDTH; ++x) {
        for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
            if (!isColor(board.get(x,y))) continue;
            const bool u = same(board,x,y,x,y+1);
            const bool d = same(board,x,y,x,y-1);
            const bool l = same(board,x,y,x-1,y);
            const bool r = same(board,x,y,x+1,y);
            l3[x][y] = ((u || d) && (l || r)) || (u && d) || (l && r);
            if (l3[x][y]) ++link3;
        }
    }

    auto expanded = [&](int x, int y) {
        if (l3[x][y]) return true;
        constexpr int dx[4] = {1,-1,0,0};
        constexpr int dy[4] = {0,0,1,-1};
        for (int d=0; d<4; ++d) {
            const int nx=x+dx[d], ny=y+dy[d];
            if (nx>=0 && nx<BOARD_WIDTH && ny>=0 && ny<VISIBLE_HEIGHT && l3[nx][ny]) return true;
        }
        return false;
    };

    for (int x = 0; x < BOARD_WIDTH; ++x) {
        for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
            // ama's l2 mask is (u | l) with l3 expanded out.
            if (!isColor(board.get(x,y)) || expanded(x,y)) continue;
            const bool u = same(board,x,y,x,y+1);
            const bool l = same(board,x,y,x-1,y);
            if (u) ++link2;
            if (l) ++link2;
        }
    }
}

double getShape(const std::array<int, BOARD_WIDTH>& h) {
    int sum = 0;
    for (int v : h) sum += v;
    int avg = sum / BOARD_WIDTH;

    static constexpr int coef[BOARD_WIDTH] = {1, 1, 1, -1, -1, -1};

    double shape = 0.0;
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        shape += std::abs(h[x] - avg - coef[x]);
    }
    return shape;
}

double getWell(const std::array<int, BOARD_WIDTH>& h) {
    double well = 0.0;

    if (h[0] < h[1]) {
        well += h[1] - h[0];
    }

    if (h[5] < h[4]) {
        well += h[4] - h[5];
    }

    for (int i = 1; i < 5; ++i) {
        if (h[i] < h[i - 1] && h[i] < h[i + 1]) {
            well += std::min(h[i - 1], h[i + 1]) - h[i];
        }
    }

    return well;
}



double getChainPotential(const Board& board) {
    // Measures how easily existing 2/3-puyo groups can be extended without
    // immediately firing unrelated groups.  A size-3 group is much more
    // valuable because one adjacent puyo can trigger it.
    bool visited[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    double potential = 0.0;

    for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (visited[x][y] || !isColor(board.get(x, y))) continue;

            const Cell c = board.get(x, y);
            std::vector<std::pair<int, int>> cells;
            std::vector<std::pair<int, int>> stack{{x, y}};
            visited[x][y] = true;

            while (!stack.empty()) {
                const auto [cx, cy] = stack.back();
                stack.pop_back();
                cells.push_back({cx, cy});

                constexpr int dx[4] = {1, -1, 0, 0};
                constexpr int dy[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    const int nx = cx + dx[d];
                    const int ny = cy + dy[d];
                    if (nx < 0 || nx >= BOARD_WIDTH ||
                        ny < 0 || ny >= VISIBLE_HEIGHT ||
                        visited[nx][ny]) continue;
                    if (board.get(nx, ny) == c) {
                        visited[nx][ny] = true;
                        stack.push_back({nx, ny});
                    }
                }
            }

            const int n = static_cast<int>(cells.size());
            if (n < 2 || n > 3) continue;

            bool extension[BOARD_WIDTH][VISIBLE_HEIGHT]{};
            int extensionCount = 0;
            for (const auto& [cx, cy] : cells) {
                constexpr int dx[4] = {1, -1, 0, 0};
                constexpr int dy[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    const int nx = cx + dx[d];
                    const int ny = cy + dy[d];
                    if (nx < 0 || nx >= BOARD_WIDTH ||
                        ny < 0 || ny >= VISIBLE_HEIGHT) continue;
                    if (board.get(nx, ny) == Cell::Empty && !extension[nx][ny]) {
                        extension[nx][ny] = true;
                        ++extensionCount;
                    }
                }
            }

            // A 3-group with many possible attachment cells is a strong
            // candidate for a future trigger.  A 2-group is useful but less
            // urgent.  The cap prevents wide-open flat boards from dominating.
            if (n == 3) {
                potential += 6.0 + std::min(extensionCount, 6) * 0.75;
            } else {
                potential += 2.0 + std::min(extensionCount, 6) * 0.25;
            }
        }
    }

    return potential;
}

double getBump(const std::array<int, BOARD_WIDTH>& h) {
    double bump = 0.0;

    for (int i = 1; i < 5; ++i) {
        if (h[i] > h[i - 1] && h[i] > h[i + 1]) {
            bump += h[i] - std::max(h[i - 1], h[i + 1]);
        }
    }

    return bump;
}




struct ConstructionMetrics {
    double unit4 = 0.0;
    double unit5 = 0.0;
    double oversized = 0.0;
    double roughness = 0.0;
    double maxStep = 0.0;
    double deadSpace = 0.0;
    double buildSpace = 0.0;
    double tailSpace = 0.0;
    double variance = 0.0;
    double edgeWall = 0.0;
};


struct SimpleGroup {
    Cell color = Cell::Empty;
    int size = 0;
    std::array<std::pair<int,int>, 72> cells{};
    int cellCount = 0;
};

std::vector<SimpleGroup> colorGroups(const Board& board) {
    std::vector<SimpleGroup> out;
    out.reserve(18);
    bool seen[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    constexpr int dx[4] = {1,-1,0,0};
    constexpr int dy[4] = {0,0,1,-1};
    for (int y=0; y<VISIBLE_HEIGHT; ++y) {
        for (int x=0; x<BOARD_WIDTH; ++x) {
            if (seen[x][y] || !isColor(board.get(x,y))) continue;
            SimpleGroup g;
            g.color = board.get(x,y);
            std::array<std::pair<int,int>, 72> stack{};
            int top=0;
            stack[top++]={x,y};
            seen[x][y]=true;
            while (top>0) {
                auto [cx,cy]=stack[--top];
                if (g.cellCount < static_cast<int>(g.cells.size()))
                    g.cells[g.cellCount++]={cx,cy};
                ++g.size;
                for (int d=0; d<4; ++d) {
                    const int nx=cx+dx[d], ny=cy+dy[d];
                    if (nx<0 || nx>=BOARD_WIDTH || ny<0 || ny>=VISIBLE_HEIGHT || seen[nx][ny]) continue;
                    if (board.get(nx,ny)==g.color) {
                        seen[nx][ny]=true;
                        stack[top++]={nx,ny};
                    }
                }
            }
            out.push_back(g);
        }
    }
    return out;
}

int reachableSlotsForGroup(const Board& board, const SimpleGroup& g) {
    const auto h=board.heights();
    bool used[BOARD_WIDTH][VISIBLE_HEIGHT]{};
    int count=0;
    constexpr int dx[4]={1,-1,0,0};
    constexpr int dy[4]={0,0,1,-1};
    for (int i=0; i<g.cellCount; ++i) {
        const auto [x,y]=g.cells[i];
        for (int d=0; d<4; ++d) {
            const int nx=x+dx[d], ny=y+dy[d];
            if (nx<0 || nx>=BOARD_WIDTH || ny<0 || ny>=VISIBLE_HEIGHT) continue;
            if (board.get(nx,ny)!=Cell::Empty || h[nx]!=ny) continue;
            if (!used[nx][ny]) { used[nx][ny]=true; ++count; }
        }
    }
    return count;
}

ConstructionMetrics getConstructionMetrics(const Board& board) {
    ConstructionMetrics m;
    const auto h = board.heights();
    const auto groups = colorGroups(board);

    // A stable Puyo board cannot contain a 4/5 group without immediately
    // firing. Therefore these two metrics measure *latent* 4/5 chain-unit
    // potential: exact-3 anchors that can become a 4/5 group with one legal
    // attachment. This matches the human construction idea without
    // accidentally rewarding already-triggered boards.
    for (const auto& g : groups) {
        const int n = g.size;
        if (n == 3) {
            const int slots = reachableSlotsForGroup(board, g);
            if (slots >= 1) m.unit4 += 1.0;
            if (slots >= 2) m.unit5 += 1.0;
        } else if (n > 5) {
            m.oversized += static_cast<double>(n - 5);
        }
    }

    // 2+2 preparation: two exact pairs of the same colour that can be joined
    // by a single physically reachable cell. This is a useful latent unit but
    // is kept weaker than a direct exact-3 anchor.
    struct PairInfo { Cell c; SimpleGroup g; };
    std::vector<PairInfo> pairs;
    for (const auto& g : groups) {
        if (g.size == 2) pairs.push_back({g.color, g});
    }
    for (std::size_t i=0; i<pairs.size(); ++i) {
        for (std::size_t j=i+1; j<pairs.size(); ++j) {
            if (pairs[i].c != pairs[j].c) continue;
            bool bridge=false;
            for (int ai=0; ai<pairs[i].g.cellCount; ++ai) {
                const auto [ax,ay] = pairs[i].g.cells[ai];
                for (int bj=0; bj<pairs[j].g.cellCount; ++bj) {
                    const auto [bx,by] = pairs[j].g.cells[bj];
                    const int dist=std::abs(ax-bx)+std::abs(ay-by);
                    if (dist != 2) continue;
                    const int mx=(ax+bx)/2, my=(ay+by)/2;
                    if (board.get(mx,my)==Cell::Empty && h[mx]==my) bridge=true;
                }
            }
            if (bridge) m.unit4 += 0.45;
        }
    }

    int totalDiff=0;
    for (int x=0; x<BOARD_WIDTH-1; ++x) {
        const int d=std::abs(h[x+1]-h[x]);
        totalDiff += d;
        m.maxStep = std::max(m.maxStep, static_cast<double>(d));
    }
    // Raw adjacent differences are more interpretable than variance for the
    // six-column field. Cap the metric so a pathological board cannot dominate.
    m.roughness = std::min(36.0, static_cast<double>(totalDiff));

    double mean=0.0;
    for (int v : h) mean += v;
    mean /= BOARD_WIDTH;
    for (int v : h) {
        const double d=v-mean;
        m.variance += d*d;
    }
    m.variance /= BOARD_WIDTH;

    // Count holes below the top occupied cell. Normal simulator states have
    // zero holes; this remains useful for edited/debug boards and protects the
    // search from creating structurally invalid dead pockets.
    for (int x=0; x<BOARD_WIDTH; ++x) {
        const int top=h[x];
        for (int y=0; y<top; ++y)
            if (board.get(x,y)==Cell::Empty) m.deadSpace += 1.0;
    }

    // Build space is not "more empty is always better": only the first six
    // free cells above each column count, and columns already at the danger
    // height receive no bonus. This preserves room without rewarding an empty
    // board over a prepared chain.
    for (int x=0; x<BOARD_WIDTH; ++x) {
        if (h[x] >= 11) continue;
        m.buildSpace += std::min(6, VISIBLE_HEIGHT - h[x]);
    }

    // Tail space: top landing cells whose neighboring surface is within one
    // row. These are the flat receiving areas that let a chain tail and the
    // next main-chain unit coexist.
    for (int x=0; x<BOARD_WIDTH; ++x) {
        const int y=h[x];
        if (y >= VISIBLE_HEIGHT) continue;
        int neighborCount=0;
        if (x>0 && std::abs(h[x-1]-h[x])<=1) ++neighborCount;
        if (x+1<BOARD_WIDTH && std::abs(h[x+1]-h[x])<=1) ++neighborCount;
        if (neighborCount>0) m.tailSpace += 1.0 + 0.5*neighborCount;
    }

    // Prefer both edges to act as mild walls only when they are higher than
    // the center and neither edge is itself dangerously high.
    const double left = h[0] + h[1];
    const double right = h[4] + h[5];
    const double center = h[2] + h[3];
    const double wall = std::max(0.0, std::min(left,right) - center*0.5);
    const double asym = std::abs(left-right);
    m.edgeWall = std::clamp(wall - 0.25*asym, 0.0, 12.0);

    return m;
}

} // namespace

Features extractStaticFeatures(const Board& board) {
    Features f;
    const auto h = board.heights();

    f.shape = getShape(h);
    f.well = getWell(h);
    f.bump = getBump(h);

    int link2 = 0, link3 = 0;
    getLink23(board, link2, link3);
    f.link2 = link2;
    f.link3 = link3;

    int garbage = 0;
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
            if (board.get(x, y) == Cell::Garbage) ++garbage;
        }
    }
    f.nuisance = garbage;

    // ama evaluates the reachable cells on row 14. The current simulator
    // uses y=13 as the 14th row.
    int row14Mask = 0;
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        if (board.get(x, BOARD_HEIGHT - 1) != Cell::Empty) {
            row14Mask |= (1 << x);
        }
    }

    int space = 1;
    for (int x = 3; x < 6; ++x) {
        if ((row14Mask >> x) & 1) break;
        ++space;
    }
    for (int x = 1; x >= 0; --x) {
        if ((row14Mask >> x) & 1) break;
        ++space;
    }
    f.waste14 = 6 - space;

    const double left = h[0] + h[1];
    const double right = h[3] + h[4] + h[5];
    f.side = std::max(left, right) - h[2];

    f.form = bestHumanFormScore(board);
    f.chainPotential = getChainPotential(board);

    const ConstructionMetrics cm = getConstructionMetrics(board);
    f.chainUnit4 = cm.unit4;
    f.chainUnit5 = cm.unit5;
    f.oversizedUnit = cm.oversized;
    f.surfaceRoughness = cm.roughness;
    f.maxStep = cm.maxStep;
    f.deadSpace = cm.deadSpace;
    f.buildSpace = cm.buildSpace;
    f.tailSpace = cm.tailSpace;
    f.heightVariance = cm.variance;
    f.edgeWall = cm.edgeWall;

    return f;
}

} // namespace puyo

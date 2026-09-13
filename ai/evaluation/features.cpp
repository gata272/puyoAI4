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
    double centralPeak = 0.0;
    double triggerExpansionSpace = 0.0;
    double edgeDeadEnd = 0.0;
    double futureConstructionSpace = 0.0;
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


// Score the amount of *physically reachable* construction room around the
// strongest exact-three trigger.  This is intentionally local: the evaluator
// should not reward an empty board, but it should strongly prefer a trigger
// from which the next B->A / C->B transfer can still be placed.
std::pair<double,double> triggerGeometry(const Board& board) {
    const auto groups = colorGroups(board);
    const auto h = board.heights();
    double bestSpace = 0.0;
    double bestDeadEnd = 0.0;
    double bestFuture = 0.0;

    for (const auto& g : groups) {
        if (g.size != 3) continue;

        bool used[BOARD_WIDTH][VISIBLE_HEIGHT]{};
        int attach = 0;
        int horizontalDirections = 0;
        bool leftReach = false;
        bool rightReach = false;
        int maxHeadroom = 0;

        for (int i = 0; i < g.cellCount; ++i) {
            const auto [x,y] = g.cells[i];
            constexpr int dx[4] = {1,-1,0,0};
            constexpr int dy[4] = {0,0,1,-1};
            for (int d=0; d<4; ++d) {
                const int nx=x+dx[d], ny=y+dy[d];
                if (nx<0 || nx>=BOARD_WIDTH || ny<0 || ny>=VISIBLE_HEIGHT) continue;
                if (board.get(nx,ny)!=Cell::Empty || h[nx]!=ny) continue;
                if (!used[nx][ny]) {
                    used[nx][ny]=true;
                    ++attach;
                    maxHeadroom = std::max(maxHeadroom, VISIBLE_HEIGHT - h[nx]);
                    if (nx < x) leftReach = true;
                    if (nx > x) rightReach = true;
                }
            }
        }
        horizontalDirections = static_cast<int>(leftReach) + static_cast<int>(rightReach);

        // Give the best trigger a modest bonus for having both horizontal
        // escape directions. Vertical-only escape is fragile near a wall.
        const double space = std::min(10.0, attach * 1.6 + horizontalDirections * 2.2
                                      + std::min(maxHeadroom, 6) * 0.35);

        int cx = 0;
        int loX = BOARD_WIDTH, hiX = -1;
        for (int i=0;i<g.cellCount;++i) {
            loX = std::min(loX, g.cells[i].first);
            hiX = std::max(hiX, g.cells[i].first);
            cx += g.cells[i].first;
        }
        cx = static_cast<int>(std::lround(static_cast<double>(cx) / g.cellCount));

        const bool edge = (loX == 0 || hiX == BOARD_WIDTH-1);
        const bool oneSided = horizontalDirections < 2;
        double deadEnd = 0.0;
        if (edge && oneSided) deadEnd += 4.0;
        if (attach <= 1) deadEnd += 3.0;
        if (h[cx] >= 9) deadEnd += 1.5;
        if (std::min(h[0], h[5]) >= 11) deadEnd += 1.0;

        // Hypothetical trigger removal: measure whether the columns occupied
        // by the trigger become useful receiving space after gravity. This is
        // a cheap structural proxy rather than a full chain simulation.
        Board after = board;
        for (int i=0;i<g.cellCount;++i)
            after.set(g.cells[i].first, g.cells[i].second, Cell::Empty);
        for (int x=0;x<BOARD_WIDTH;++x) {
            int writeY=0;
            for (int y=0;y<BOARD_HEIGHT;++y) {
                const Cell c=after.get(x,y);
                if (c!=Cell::Empty) after.set(x,writeY++,c);
            }
            while (writeY<BOARD_HEIGHT) after.set(x,writeY++,Cell::Empty);
        }
        const auto ah = after.heights();
        double future = 0.0;
        for (int x=std::max(0,loX-1); x<=std::min(BOARD_WIDTH-1,hiX+1); ++x) {
            if (ah[x] >= 11) continue;
            future += std::min(4, VISIBLE_HEIGHT-ah[x]) * 0.5;
        }

        if (space > bestSpace || (space == bestSpace && future > bestFuture)) {
            bestSpace = space;
            bestDeadEnd = deadEnd;
            bestFuture = std::min(10.0, future);
        }
    }
    return {bestSpace, bestDeadEnd + std::max(0.0, 4.0-bestSpace)*0.5};
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

    double excessSlope = 0.0;
    for (int x=0; x<BOARD_WIDTH-1; ++x) {
        // A one-row slope is considered normal construction geometry. Penalize
        // only sharper steps, so a gentle 6-5-4-4-5-6 edge-wall shape is not
        // treated as rough while a 1-5-1-5 surface remains strongly bad.
        const double d = static_cast<double>(std::abs(h[x+1]-h[x]));
        const double excess = std::max(0.0, d - 1.0);
        excessSlope += excess;
        m.maxStep = std::max(m.maxStep, excess);
    }
    m.roughness = std::min(24.0, excessSlope);

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
    const double left = (h[0] + h[1]) / 2.0;
    const double right = (h[4] + h[5]) / 2.0;
    const double center = (h[2] + h[3]) / 2.0;
    const double wall = std::max(0.0, std::min(left, right) - center);
    const double asym = std::abs(left-right);
    m.edgeWall = std::clamp(wall - 0.20*asym, 0.0, 6.0);

    // Penalize the specific geometry that repeatedly trapped the main chain:
    // a high central wall with comparatively low edges.  Flatness alone is
    // not the target; a gentle edge-high / center-low profile remains good.
    const double edgeMean = (h[0] + h[1] + h[4] + h[5]) / 4.0;
    const double centerMean = (h[2] + h[3]) / 2.0;
    const double centerExcess = std::max(0.0, centerMean - edgeMean - 1.0);
    const double centerPeak = std::max(0.0,
        std::max(h[2], h[3]) - std::max(h[0], std::max(h[1], std::max(h[4], h[5]))) - 1.0);
    m.centralPeak = std::clamp(centerExcess * 1.5 + centerPeak * 0.75, 0.0, 12.0);

    const auto tg = triggerGeometry(board);
    m.triggerExpansionSpace = tg.first;
    m.edgeDeadEnd = tg.second;
    // The general receiving-space signal is deliberately capped and does not
    // count all empty cells. It rewards room in the neighborhood where a
    // main-chain transfer is most likely to be built.
    m.futureConstructionSpace = std::clamp(
        m.buildSpace * 0.12 + m.tailSpace * 0.55 - m.centralPeak * 0.35,
        0.0, 18.0);

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
    f.centralPeak = cm.centralPeak;
    f.triggerExpansionSpace = cm.triggerExpansionSpace;
    f.edgeDeadEnd = cm.edgeDeadEnd;
    f.futureConstructionSpace = cm.futureConstructionSpace;

    return f;
}

} // namespace puyo

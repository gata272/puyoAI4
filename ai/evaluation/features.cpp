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

double getBump(const std::array<int, BOARD_WIDTH>& h) {
    double bump = 0.0;

    for (int i = 1; i < 5; ++i) {
        if (h[i] > h[i - 1] && h[i] > h[i + 1]) {
            bump += h[i] - std::max(h[i - 1], h[i + 1]);
        }
    }

    return bump;
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

    return f;
}

} // namespace puyo

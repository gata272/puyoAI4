#include "forms.h"

#include <algorithm>
#include <array>

namespace puyo {
namespace {

struct Pattern {
    int form[6][6]{}; // y=0 is bottom
    int matrix[9][9]{};
    int groups = 0;
};

Pattern makePattern(const int top[6][6], const int srcMatrix[9][9], int groups) {
    Pattern p;
    p.groups = groups;
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 6; ++x) {
            p.form[y][x] = top[5 - y][x];
        }
    }
    for (int y = 0; y < 9; ++y)
        for (int x = 0; x < 9; ++x)
            p.matrix[y][x] = srcMatrix[y][x];
    return p;
}

const Pattern& gtr() {
    static const Pattern p = [] {
        constexpr int form[6][6] = {
            {0,0,0,0,0,0},
            {4,4,4,0,0,0},
            {3,3,3,4,0,0},
            {1,2,5,0,0,0},
            {1,1,2,5,0,0},
            {2,2,5,0,0,0}
        };
        constexpr int m[9][9] = {
            {0,0,0,0,0,0,0,0,0},
            {0,2,-1,-1,0,0,0,0,0},
            {0,-1,1,-1,0,-1,0,0,0},
            {0,-1,-1,2,-1,-1,0,0,0},
            {0,-1,-1,-1,2,-1,0,0,0},
            {0,0,-1,-1,0,0,0,0,0},
            {0,0,0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0,0,0}
        };
        return makePattern(form,m,5);
    }();
    return p;
}

const Pattern& sgtr() {
    static const Pattern p = [] {
        constexpr int form[6][6] = {
            {0,0,0,0,0,0},
            {5,5,5,0,0,0},
            {4,4,4,5,0,0},
            {1,1,3,6,0,0},
            {1,2,2,3,6,0},
            {2,3,3,6,0,0}
        };
        constexpr int m[9][9] = {
            {0,0,0,0,0,0,0,0,0},
            {0,2,-1,-1,-1,0,0,0,0},
            {0,-1,1,-1,0,0,0,0,0},
            {0,-1,-1,1,-1,0,-1,0,0},
            {0,-1,0,-1,2,-1,0,0,0},
            {0,0,0,0,-1,0,0,0,0},
            {0,0,0,-1,0,0,0,0,0},
            {0,0,0,0,0,0,0,0,0},
            {0,0,0,0,0,0,0,0,0}
        };
        return makePattern(form,m,6);
    }();
    return p;
}

const Pattern& fron() {
    static const Pattern p = [] {
        constexpr int form[6][6] = {
            {0,0,0,0,0,0},
            {5,5,5,0,0,0},
            {4,4,4,5,0,0},
            {1,1,3,6,0,0},
            {1,2,2,7,0,0},
            {3,3,2,3,6,0}
        };
        constexpr int m[9][9] = {
            {0,0,0,0,0,0,0,0,0},
            {0,2,-1,-1,-1,0,0,0,0},
            {0,-1,1,-1,0,0,0,-1,0},
            {0,-1,-1,1,-1,0,-1,0,0},
            {0,-1,0,-1,2,-1,0,0,0},
            {0,0,0,0,-1,0,0,0,0},
            {0,0,0,-1,0,0,0,0,0},
            {0,0,-1,0,0,0,0,0,0},
            {0,0,0,0,0,0,0,0,0}
        };
        return makePattern(form,m,7);
    }();
    return p;
}

int evaluate(const Board& board, const Pattern& p) {
    std::array<Cell, 9> map{};
    map.fill(Cell::Empty);
    int result = 0;

    const auto h = board.heights();
    for (int x = 0; x < 6; ++x) {
        for (int y = 0; y < 6; ++y) {
            if (h[x] <= y) break;
            const int idx = p.form[y][x];
            if (idx == 0) continue;
            const Cell c = board.get(x,y);
            if (map[idx] == Cell::Empty) {
                map[idx] = c;
            } else if (map[idx] != c && p.matrix[idx][idx] > 0) {
                return -100;
            }
            result += p.matrix[idx][idx];
        }
    }

    for (int i = 1; i < p.groups; ++i) {
        if (map[i] == Cell::Empty) continue;
        for (int k = i + 1; k <= p.groups; ++k) {
            if (map[k] == Cell::Empty) continue;
            const int rel = p.matrix[i][k];
            if (rel < 0 && map[i] == map[k]) return -100;
            if (rel > 0) {
                if (map[i] != map[k]) return -100;
                result += rel;
            }
        }
    }
    return result;
}

} // namespace

double bestHumanFormScore(const Board& board) {
    // Match ama's behavior: garbage in the visible field disables pattern
    // matching. Hidden-row garbage is not relevant to the evaluator.
    for (int x = 0; x < BOARD_WIDTH; ++x)
        for (int y = 0; y < VISIBLE_HEIGHT; ++y)
            if (board.get(x,y) == Cell::Garbage)
                return 0.0;

    return std::max({
        evaluate(board, gtr()),
        evaluate(board, fron()),
        evaluate(board, sgtr())
    });
}

} // namespace puyo

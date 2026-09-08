#include "simulator.h"

#include <algorithm>
#include <array>
#include <queue>
#include <vector>

namespace puyo {

namespace {

struct Coord {
    int x;
    int y;
};

bool sameColor(Cell c) {
    return c != Cell::Empty && c != Cell::Garbage;
}

std::vector<Coord> coordsFor(const PuyoPair& p, int x, int y, int rotation) {
    int sx = x;
    int sy = y;

    switch (rotation & 3) {
        case 0: sy = y + 1; break; // sub above
        case 1: sx = x - 1; break; // sub left
        case 2: sy = y - 1; break; // sub below
        case 3: sx = x + 1; break; // sub right
    }

    return {{x, y}, {sx, sy}};
}

} // namespace

bool Simulator::canPlace(
    const Board& board,
    const PuyoPair& pair,
    int x,
    int y,
    int rotation
) {
    for (const auto& c : coordsFor(pair, x, y, rotation)) {
        if (c.x < 0 || c.x >= BOARD_WIDTH) return false;
        if (c.y < 0 || c.y >= BOARD_HEIGHT) return false;
        if (board.get(c.x, c.y) != Cell::Empty) return false;
    }
    return true;
}

int Simulator::findDropY(
    const Board& board,
    const PuyoPair& pair,
    int x,
    int rotation
) {
    int y = BOARD_HEIGHT - 2; // same spawn anchor as puyoSim.js
    if (!canPlace(board, pair, x, y, rotation)) return -1;

    while (y > 0 && canPlace(board, pair, x, y - 1, rotation)) {
        --y;
    }
    return y;
}

void Simulator::gravity(Board& board) {
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        int writeY = 0;
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            Cell c = board.get(x, y);
            if (c != Cell::Empty) {
                board.set(x, writeY++, c);
            }
        }
        while (writeY < BOARD_HEIGHT) {
            board.set(x, writeY++, Cell::Empty);
        }
    }
}

int Simulator::resolve(Board& board, int& score, int& erased) {
    static constexpr int chainBonus[] = {
        0, 8, 16, 32, 64, 96, 128, 160, 192,
        224, 256, 288, 320, 352, 384, 416, 448, 480, 512
    };
    static constexpr int groupBonus[] = {
        0, 0, 0, 0, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
    };
    static constexpr int colorBonus[] = {0, 0, 3, 6, 12};

    int chains = 0;

    while (true) {
        gravity(board);

        std::array<std::array<bool, BOARD_HEIGHT>, BOARD_WIDTH> visited{};
        std::vector<std::vector<Coord>> groups;

        for (int y = 0; y < VISIBLE_HEIGHT; ++y) {
            for (int x = 0; x < BOARD_WIDTH; ++x) {
                if (visited[x][y]) continue;
                Cell color = board.get(x, y);
                if (!sameColor(color)) continue;

                std::vector<Coord> group;
                std::queue<Coord> q;
                q.push({x, y});
                visited[x][y] = true;

                while (!q.empty()) {
                    Coord cur = q.front();
                    q.pop();
                    group.push_back(cur);

                    constexpr int dx[] = {1, -1, 0, 0};
                    constexpr int dy[] = {0, 0, 1, -1};

                    for (int d = 0; d < 4; ++d) {
                        int nx = cur.x + dx[d];
                        int ny = cur.y + dy[d];
                        if (nx < 0 || nx >= BOARD_WIDTH ||
                            ny < 0 || ny >= VISIBLE_HEIGHT ||
                            visited[nx][ny]) {
                            continue;
                        }
                        if (board.get(nx, ny) == color) {
                            visited[nx][ny] = true;
                            q.push({nx, ny});
                        }
                    }
                }

                if (group.size() >= 4) {
                    groups.push_back(std::move(group));
                }
            }
        }

        if (groups.empty()) break;

        ++chains;
        int groupCount = 0;
        int colorCount = 0;
        std::array<bool, 6> colors{};

        std::vector<Coord> eraseCells;
        for (const auto& group : groups) {
            groupCount += static_cast<int>(group.size());
            Cell c = board.get(group.front().x, group.front().y);
            colors[static_cast<int>(c)] = true;
            eraseCells.insert(eraseCells.end(), group.begin(), group.end());
        }

        for (int i = 1; i < 5; ++i) {
            if (colors[i]) ++colorCount;
        }

        int gb = groupBonus[std::min(groupCount, 15)];
        int cb = colorBonus[std::min(colorCount, 4)];
        int cbn = chainBonus[std::min(chains, 18)];
        int multiplier = std::max(1, gb + cb + cbn);

        score += groupCount * 10 * multiplier;
        erased += groupCount;

        // Mark erased cells first so adjacent garbage is removed together.
        for (const auto& c : eraseCells) {
            board.set(c.x, c.y, Cell::Empty);
        }

        std::vector<Coord> garbageToErase;
        for (const auto& c : eraseCells) {
            constexpr int dx[] = {1, -1, 0, 0};
            constexpr int dy[] = {0, 0, 1, -1};
            for (int d = 0; d < 4; ++d) {
                int nx = c.x + dx[d];
                int ny = c.y + dy[d];
                if (nx < 0 || nx >= BOARD_WIDTH ||
                    ny < 0 || ny >= BOARD_HEIGHT) {
                    continue;
                }
                if (board.get(nx, ny) == Cell::Garbage) {
                    garbageToErase.push_back({nx, ny});
                }
            }
        }

        std::sort(
            garbageToErase.begin(),
            garbageToErase.end(),
            [](const Coord& a, const Coord& b) {
                return a.y * BOARD_WIDTH + a.x <
                       b.y * BOARD_WIDTH + b.x;
            }
        );
        garbageToErase.erase(
            std::unique(
                garbageToErase.begin(),
                garbageToErase.end(),
                [](const Coord& a, const Coord& b) {
                    return a.x == b.x && a.y == b.y;
                }
            ),
            garbageToErase.end()
        );

        for (const auto& c : garbageToErase) {
            board.set(c.x, c.y, Cell::Empty);
        }
    }

    gravity(board);
    return chains;
}

SimulationResult Simulator::drop(
    const Board& board,
    const PuyoPair& pair,
    const Move& move
) {
    SimulationResult result;
    result.board = board;

    int y = findDropY(result.board, pair, move.x, move.rotation);
    if (y < 0) {
        result.gameOver = true;
        result.score = -1000000;
        return result;
    }

    for (const auto& c : coordsFor(pair, move.x, y, move.rotation)) {
        result.board.set(
            c.x, c.y,
            c.x == move.x && c.y == y
                ? static_cast<Cell>(pair.main)
                : static_cast<Cell>(pair.sub)
        );
    }

    // Match puyoSim.js: the top hidden row is not used for chaining.
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        result.board.set(x, BOARD_HEIGHT - 1, Cell::Empty);
    }

    result.chains = resolve(result.board, result.score, result.erased);

    result.allClear = true;
    for (int x = 0; x < BOARD_WIDTH && result.allClear; ++x) {
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            if (result.board.get(x, y) != Cell::Empty) {
                result.allClear = false;
                break;
            }
        }
    }

    result.gameOver = result.board.gameOver();
    return result;
}

} // namespace puyo

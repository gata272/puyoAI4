#include <emscripten/emscripten.h>

#include "../ai/ai.h"
#include "../ai/benchmark/chain_benchmark.h"

#include <array>
#include <vector>
#include <string>

static std::string g_benchmarkResult;

namespace {

puyo::AI g_ai;
puyo::Board g_board;

puyo::Cell decodeCell(int value) {
    if (value < 0 || value > 5) return puyo::Cell::Empty;
    return static_cast<puyo::Cell>(value);
}

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
void reset_ai() {
    g_ai.reset();
    g_board.clear();
}

EMSCRIPTEN_KEEPALIVE
void set_board_cell(int index, int value) {
    if (index < 0 || index >= puyo::BOARD_WIDTH * puyo::BOARD_HEIGHT) {
        return;
    }

    const int x = index % puyo::BOARD_WIDTH;
    const int y = index / puyo::BOARD_WIDTH;
    g_board.set(x, y, decodeCell(value));
}

EMSCRIPTEN_KEEPALIVE
int ai_choose_move(
    int turn,
    int sub1, int main1,
    int sub2, int main2,
    int sub3, int main3
) {
    std::vector<puyo::PuyoPair> pieces = {
        {main1, sub1},
        {main2, sub2},
        {main3, sub3}
    };

    puyo::Move move = g_ai.chooseMove(
        turn,
        g_board,
        pieces
    );

    if (!move.valid) return -1;

    // x * 10 + rotation; compatible with the old JS bridge.
    return move.x * 10 + move.rotation;
}

EMSCRIPTEN_KEEPALIVE
const char* run_chain_benchmark(
    int games,
    int turns,
    int seed,
    int depth,
    int beamWidth
) {
    puyo::ChainBenchmarkConfig config;
    config.games = games;
    config.turns = turns;
    config.seed = seed;
    config.depth = depth;
    config.beamWidth = beamWidth;
    g_benchmarkResult = puyo::runChainBenchmark(config);
    return g_benchmarkResult.c_str();
}

EMSCRIPTEN_KEEPALIVE
const char* get_ai_pattern_name() {
    return g_ai.patternName();
}

}

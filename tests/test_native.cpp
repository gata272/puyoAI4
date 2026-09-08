
#include "../ai/simulation/simulator.h"
#include "../ai/ai.h"
#include <cassert>
#include <iostream>

int main() {
    puyo::Board board;
    puyo::AI ai;

    std::vector<puyo::PuyoPair> pieces = {
        {1, 1}, {1, 2}, {1, 2}
    };

    auto move = ai.chooseMove(0, board, pieces);
    assert(move.valid);
    assert(move.x >= 0 && move.x < puyo::BOARD_WIDTH);
    assert(move.rotation >= 0 && move.rotation < 4);

    auto sim = puyo::Simulator::drop(board, pieces[0], move);
    assert(!sim.gameOver);

    auto next = ai.chooseMove(3, sim.board, pieces);
    assert(next.valid);

    std::cout << "native AI smoke test passed: "
              << next.x << "," << next.rotation << "\n";
    return 0;
}


#include "../ai/simulation/simulator.h"
#include "../ai/ai.h"
#include "../ai/evaluation/trigger_route.h"
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

    // Regression test: when every legal placement is a game-over placement,
    // the AI must still return a valid move rather than refusing to move.
    puyo::Board doomed;
    for (int x = 0; x < puyo::BOARD_WIDTH; ++x) {
        for (int y = 0; y < puyo::VISIBLE_HEIGHT; ++y) {
            doomed.set(x, y, static_cast<puyo::Cell>(1 + ((x + y) % 4)));
        }
    }
    auto fallback = ai.chooseMove(3, doomed, pieces, 4, 8);
    assert(fallback.valid);
    auto fallbackSim = puyo::Simulator::drop(doomed, pieces[0], fallback);
    assert(fallbackSim.gameOver);
    assert(!fallbackSim.allClear);


    // Trigger-transfer regression: C -> B -> A is recognized as a three-level
    // dependency.  Clearing C makes B a four-group; clearing B then makes A
    // a four-group.
    puyo::Board relay;
    relay.set(1,0,puyo::Cell::Red); relay.set(2,0,puyo::Cell::Red); relay.set(3,0,puyo::Cell::Red);
    relay.set(1,1,puyo::Cell::Blue); relay.set(2,1,puyo::Cell::Blue); relay.set(1,2,puyo::Cell::Blue);
    relay.set(2,2,puyo::Cell::Red);
    relay.set(0,3,puyo::Cell::Green); relay.set(1,3,puyo::Cell::Green); relay.set(0,4,puyo::Cell::Green);
    relay.set(1,4,puyo::Cell::Blue);
    assert(puyo::triggerRouteLength(relay) >= 3);

    // Horizontal transfer regression: clearing a vertical B trigger lets an
    // upper A fall beside an existing horizontal AAA, proving the dependency
    // detector is not restricted to the vertical motif.
    puyo::Board horizontal;
    horizontal.set(1,0,puyo::Cell::Red);
    horizontal.set(2,0,puyo::Cell::Red);
    horizontal.set(3,0,puyo::Cell::Red);
    horizontal.set(0,0,puyo::Cell::Blue);
    horizontal.set(0,1,puyo::Cell::Blue);
    horizontal.set(0,2,puyo::Cell::Blue);
    horizontal.set(0,3,puyo::Cell::Red);
    assert(puyo::triggerRouteLength(horizontal) >= 2);

    std::cout << "native AI smoke test passed: "
              << next.x << "," << next.rotation << "\n";
    return 0;
}

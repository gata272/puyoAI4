#include "move_generator.h"

namespace puyo {

std::vector<Move> generateLegalMoves(
    const Board& board,
    const PuyoPair& pair
) {
    std::vector<Move> moves;

    for (int rotation = 0; rotation < 4; ++rotation) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (Simulator::findDropY(board, pair, x, rotation) >= 0) {
                moves.push_back({x, rotation, true});
            }
        }
    }

    return moves;
}

} // namespace puyo

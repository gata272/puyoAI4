#pragma once

#include "../simulation/board.h"
#include "../simulation/simulator.h"
#include "../search/move.h"

#include <vector>

namespace puyo {

// Number of color-to-color trigger dependencies currently present in the
// board.  A route such as C -> B -> A has length 3.
int triggerRouteLength(const Board& board);

// Static structural value used by the ordinary evaluator.
double triggerRelayScore(const Board& board);

// Value of the strongest marked exact-3 trigger.
double triggerAnchorValue(const Board& board);

// Queue-aware value.  The search only uses the supplied future pairs, so this
// does not give the AI hidden information beyond its visible horizon.
double triggerQueueScore(const Board& board, const std::vector<PuyoPair>& pieces);

double triggerRouteScore(const Board& board);

} // namespace puyo

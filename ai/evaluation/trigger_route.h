#pragma once

#include "../simulation/board.h"

namespace puyo {

// Measures the longest color-to-color trigger-transfer route that is already
// encoded in the board.  A -> B means: fire a 3-puyo A trigger with an A/B
// vertical or horizontal pair, then the B puyo falls/connects to a 3-puyo B
// group and becomes the next trigger.  This is deliberately different from
// generic "chain potential": it represents an ordered trigger dependency.
int triggerRouteLength(const Board& board);

double triggerRouteScore(const Board& board);

} // namespace puyo

#pragma once

#include "../simulation/board.h"

namespace puyo {

// Human-form patterns ported from ama's public beam/form.h:
// GTR, SGTR and FRON.  The matcher is color-agnostic and checks the
// equality/inequality relations encoded by each pattern.
double bestHumanFormScore(const Board& board);

} // namespace puyo

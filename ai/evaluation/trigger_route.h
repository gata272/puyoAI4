#pragma once

#include "../simulation/board.h"

namespace puyo {

// Legacy trigger-route metric. Kept for compatibility with the beam search,
// but now backed by the trigger-relay structure below.
int triggerRouteLength(const Board& board);

// Score for the user's trigger-relay construction:
//
//   A A A          <- an existing 3-puyo A trigger
//     B
//     A            <- B -> A is stacked above it
//
// The A is intentionally separated from the lower A group by B.  When B is
// fired elsewhere on a later turn, B disappears and the upper A falls onto
// the lower A group, making A into 4 and causing the next chain.
//
// The score also prefers B positions that already have a 1- or 2-puyo B
// support group, because only a small number of later B drops are then needed
// to fire B.  A support group of 3 is not rewarded: placing the B would make
// four immediately and destroy the intended delayed relay.
double triggerRelayScore(const Board& board);

// Compatibility helper used by older code.
double triggerRouteScore(const Board& board);

} // namespace puyo

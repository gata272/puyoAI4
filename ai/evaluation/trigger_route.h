#pragma once

#include "../simulation/board.h"
#include "../search/move.h"
#include <vector>

namespace puyo {

// Structural analysis for human-style large-chain construction.
// A route is a sequence of latent 3-groups: removing the current group and
// applying gravity causes another colour to become a 4+ group.  This models
// the user's "A -> B -> C" trigger-transfer idea without looking at hidden
// future pieces.
int triggerRouteLength(const Board& board);

double triggerRelayScore(const Board& board);
double triggerAnchorValue(const Board& board);
double triggerQueueScore(const Board& board, const std::vector<PuyoPair>& pieces);
double triggerRouteScore(const Board& board);

// Additional structural terms used by the large-chain evaluator.
//
// preparedGroupScore: values 3+1 / 2+2 style groups that are not currently
// firing but can become 4+ after a trigger elsewhere fires.
// postTriggerTailScore: compares the board before a trigger with the board
// after one or more real chain waves and rewards cells/groups that only become
// removable after the trigger.
// prematureTriggerRisk: penalizes placements that turn a valuable latent
// anchor into an immediately firing 4+ group before the intended route is
// constructed.
double preparedGroupScore(const Board& board);
double postTriggerTailScore(const Board& board);
double prematureTriggerRisk(const Board& board);

} // namespace puyo

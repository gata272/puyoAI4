#pragma once

#include "../simulation/board.h"
#include "../search/move.h"
#include <vector>

namespace puyo {

// Large-chain construction is treated as a dependency PATH, not as a graph
// whose branches are rewarded.  A path step means: removing the previous
// trigger causes a new group to become removable on a later chain wave.
// The same colour may appear more than once (A -> B -> A -> C); nodes are
// chain events, not colours.
int triggerRouteLength(const Board& board);
double triggerRelayScore(const Board& board);
double triggerAnchorValue(const Board& board);
double triggerQueueScore(const Board& board, const std::vector<PuyoPair>& pieces);
double triggerRouteScore(const Board& board);

// Scores for the construction policy described in the project notes.
// preparedGroupScore rewards useful latent 3 / pair structures while avoiding
// immediate 4+ firing. postTriggerTailScore rewards material that becomes
// removable only after the hypothetical trigger and its subsequent gravity.
// prematureTriggerRisk penalizes destroying a valuable latent construction.
double preparedGroupScore(const Board& board);
double postTriggerTailScore(const Board& board);
double prematureTriggerRisk(const Board& board);

// More explicit metrics for diagnostics and tuning.
// chainDependencyPathScore is intentionally based on the longest *sequential*
// chain path. Multiple groups disappearing on the same wave do not increase
// the path length. branchPenalty is a small penalty for wasting material on
// same-wave parallel removals instead of extending the next wave.
double chainDependencyPathScore(const Board& board);
double chainDependencyBranchPenalty(const Board& board);

} // namespace puyo

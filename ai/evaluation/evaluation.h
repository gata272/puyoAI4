#pragma once

#include "features.h"
#include "weights.h"
#include "../search/move.h"
#include "../simulation/simulator.h"
#include <vector>

namespace puyo {

struct EvaluationContext {
    std::vector<PuyoPair> lookahead;
    int quiescenceDepth = 3;
};

double evaluate(
    const Board& board,
    const Weights& weights,
    const EvaluationContext& context
);

double actionPenalty(
    const Board& before,
    const SimulationResult& result,
    const Move& move,
    const Weights& weights
);

} // namespace puyo

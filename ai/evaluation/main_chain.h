#pragma once

#include "../simulation/board.h"
#include <vector>

namespace puyo {

// A compact representation of the single sequential chain the AI is
// currently trying to grow.  `colors` are chain-wave colours, not a promise
// that every wave contains only one colour.  Repeated colours are allowed:
// A -> B -> A -> C is a valid plan.
struct MainChainPlan {
    std::vector<int> colors;
    int branchWaves = 0;
    int anchorX = -1;
    int anchorY = -1;

    int length() const { return static_cast<int>(colors.size()); }
    bool empty() const { return colors.empty(); }
};

// Finds the strongest sequential trigger route currently present.  The
// analysis starts from an exact-three group so the AI can preserve a prepared
// trigger instead of rewarding already-firing four-groups.
MainChainPlan analyzeMainChain(const Board& board);

// Rewards keeping one chain plan alive from parent -> child.  It deliberately
// prefers extending the same route over replacing it with unrelated triples.
double mainChainContinuityScore(
    const MainChainPlan& parent,
    const MainChainPlan& child,
    int actualChains
);

// Human-style fallback: if the main route cannot be extended, reward a quiet
// cleanup only when it preserves the route and reduces unrelated material.
double mainChainCleanupScore(
    const MainChainPlan& parent,
    const MainChainPlan& child,
    int actualChains
);

} // namespace puyo

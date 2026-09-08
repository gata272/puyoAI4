# PuyoAI architecture

## Runtime flow

1. `puyoSim.js` remains the browser game engine and online-match state owner.
2. `puyoAI.js` is only the browser/WASM bridge.
3. `puyo-ai-worker-wasm.js` isolates AI computation from the UI thread.
4. `wasm/puyoAI.cpp` is the C ABI adapter.
5. `ai/ai.cpp` chooses the strategy:
   - turns 0-2: the existing GTR planner
   - afterwards: Beam Search + linear evaluation
6. `ai/simulation/` provides a C++ board simulator used only for search.
7. `ai/evaluation/` extracts features and applies weights.
8. `ai/search/` generates legal placements and performs beam search.

## Why the browser simulator is not reused

The JavaScript simulator owns animation, undo/redo, online synchronization and user input.
The AI needs a deterministic, side-effect-free simulator for thousands of hypothetical moves.
Keeping those responsibilities separate prevents the AI from mutating the live online game.

## Search

The initial upgraded search uses:

- 3-pair lookahead
- beam width 8
- quiescence-style tactical evaluation at depth 1
- a linear weighted evaluation function

The parameters are intentionally centralized so that beam width, search depth and quiescence depth can be tuned independently later.


## Maximum-chain search

After the opening GTR phase, the AI uses a true global beam search over up to
six pairs. Immediate chain counts receive a nonlinear reward, while a chain-
potential feature values extendable 2/3-puyo groups. The final root choice is
lexicographic: maximize the largest chain observed within the search horizon,
then maximize the accumulated heuristic score.

Game-over placements are treated as a fallback class. They are excluded when
at least one safe placement exists; if every legal placement causes game over,
the best game-over placement is returned.

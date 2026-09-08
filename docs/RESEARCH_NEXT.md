# AI upgrade roadmap

## Completed in this revision

1. Human-form matcher: public ama GTR / SGTR / FRON patterns.
2. Scalar-equivalent `link_2` / `link_3` masks.
3. Ama-style quiescence with a three-puyo tactical drop depth.
4. Beam benchmark harness and explicit search configuration.
5. SPSA automatic weight-tuning harness with common random numbers.

## Still recommended

- Validate every C++ simulator transition against `puyoSim.js`, including
  wall-kick, top-row behavior, garbage removal and chain scoring.
- Add a transposition table if browser profiling shows it is worthwhile.
- Tune the production beam width on the actual deployment device rather than
  assuming that a wider beam is always stronger.
- Build a curated tactical benchmark (GTR completion, trigger selection,
  extension, nuisance handling and death avoidance) before accepting tuned
  weights.
- Compare tuned profiles against the public ama `build` profile using the same
  queue corpus.


## Maximum-chain focused revision

- Post-GTR search can consume six pairs (current + five lookahead).
- Root selection is maximum-chain-first, then normal evaluation as the tie-breaker.
- Immediate chains receive a nonlinear `chains^4` reward.
- A `chainPotential` feature rewards extendable 2/3-puyo groups.
- If safe placements exist, game-over placements are excluded. If none exist,
  the least-bad game-over placement is returned instead of reporting no move.

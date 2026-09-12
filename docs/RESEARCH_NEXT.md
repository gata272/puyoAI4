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
- Re-run the PuyoAI12 multi-seed comparison at the full 100 games x 100 turns
  x beamWidth 24-32 protocol; the numbers in that section were measured at
  reduced scale to fit available compute.
- Run `tools/tuner.cpp`'s SPSA to convergence on its new large-chain-focused
  objective and fold the result into `ai/evaluation/weights.cpp`.
- Build an explicit large-chain skeleton/template library (e.g. staircase or
  GTR-relay extensions spanning the full board height) rather than relying on
  the emergent per-move evaluator alone; PuyoAI12's attempt at a general
  "packing efficiency" proxy for this was measured to hurt and was reverted.
- Consider a learned value function (self-play RL) as the long-term
  replacement for the hand-tuned static evaluator; PuyoAI12 found evidence
  that the current evaluator cannot reliably guide delayed-gratification
  construction once the immediate chain-firing reward is turned down.


## Maximum-chain focused revision

- Main search is constrained to three pairs (current + two lookahead), matching the intended human-information limit.
- Root selection is maximum-chain-first, then normal evaluation as the tie-breaker.
- Immediate chains receive a nonlinear `chains^4` reward.
- A `chainPotential` feature rewards extendable 2/3-puyo groups.
- If safe placements exist, game-over placements are excluded. If none exist,
  the least-bad game-over placement is returned instead of reporting no move.

## Trigger relay construction (PuyoAI8.1)

The current chain-building experiment targets a concrete delayed-trigger relay:

1. Find an existing exact-3 group of color A.
2. Construct a vertical B->A pair above it, giving `A(3) / B / A`.
3. Do not fire A while constructing the relay; the separating B is intentional.
4. Build B into a trigger on a later turn by dropping additional B puyos next to the relay B.
5. When B fires, the relay B disappears and the upper A falls onto the existing A(3), making A(4), producing the next chain.

The evaluator rewards such latent relays, especially when the relay B already has a 1- or 2-puyo support group and therefore needs only a small number of future B drops to fire. A 3-puyo support is not rewarded because adding the relay B would immediately fire B and destroy the intended delayed construction.

## PuyoAI10: persistent trigger-transfer construction

PuyoAI10 changes the research objective from direct long-horizon maximum-chain search to a human-information-constrained trigger-transfer policy.

- The AI receives and uses only the current pair plus two lookahead pairs (3 pairs total).
- Exact-3 groups are treated as candidate marked triggers.
- A trigger dependency `B -> A` exists when removing an exact-3 B group and applying gravity makes an A group reach four or more.
- Dependencies are recognized in both vertical and horizontal arrangements, so motifs such as `A / BAAA` and `A / B / AAA` are both represented by the same dependency test.
- A route such as `D -> C -> B -> A` receives a strong structural reward.
- The strongest exact-3 anchor is protected from accidental destruction unless the move actually resolves a chain.
- The visible three-pair queue is used only as compatibility information: if a useful predecessor color is not present, the anchor remains valuable and the AI may wait instead of forcing a destructive construction.
- `Simulator::resolveBoard()` exposes the exact production resolution rules to the trigger planner, avoiding a second, inconsistent chain implementation.

The intention is to repeatedly move the marked trigger upward or sideways rather than spending the trigger immediately. This can build a long latent dependency chain while respecting the three-pair information limit.


## PuyoAI11: Long Chain Potential

PuyoAI11 keeps the human-information constraint of three visible pairs, but changes the search objective from immediate maximum-chain preference toward latent large-chain construction. The new evaluator scores extendable 2/3-groups, reachable extension cells, construction shape, queue compatibility and height pressure. Existing ama form matching and trigger-transfer evaluation remain active.

The beam also reserves a potential elite so a quiet but promising construction is not removed solely because its immediate evaluator score is lower. Final selection uses accumulated search score plus a moderate actual-chain bonus and latent-potential bonus instead of lexicographic maximum-chain-first selection.

This revision is deliberately conservative about hidden information: no future queue beyond the visible three pairs is read. The benchmark must be repeated on larger fixed seed corpora before claiming a statistically significant improvement.

## PuyoAI12: build/fire separation, tuned empirically (2026-09)

Starting point: 100 games x 100 turns, depth 3, beamWidth 24 -> average max chain
8.17, median 8, max 10, 0% at 12+.

Five changes were proposed to push toward a 15-chain target: (1) a build/fire
state machine that suppresses cashing out a merely-decent chain, (2) explicit
large-chain shape templates, (3) relaxed height penalties so the evaluator
stops fighting the tall stacks a big chain needs, (4) a wider, more diverse
beam, (5) an SPSA objective specialized for reaching large-chain milestones.
Each was implemented and then evaluated by running the native
`chain_benchmark_cli` head-to-head against the unmodified code on multiple
independent seeds (not just the single seed used for a quick sanity check),
because several of these changes turned out to help on one seed and hurt on
another. What shipped is narrower than what was originally proposed:

- **(1) shipped, but much narrower than first attempted.** The first version
  suppressed every chain below a high target (13). Measured result: it
  *lowered* the achieved ceiling (P(>=10) dropped, max chain dropped) because
  with only a 3-piece lookahead the search cannot tell whether refusing an
  available chain will pay off later -- suppression mostly just throws away
  the lucky windows where a good chain happens to be available. Sweeping
  `targetChain`/`dangerHeight` (via the `PUYOAI_TARGET_CHAIN` /
  `PUYOAI_DANGER_HEIGHT` env vars added for this purpose) found that a narrow
  window just above the model's typical ceiling -- soften chains 5..10,
  fully reward 11+, and always allow a full-strength fire once any column
  reaches height 11 -- was the only setting that reliably beat baseline on
  *both* average chain and P(>=10) across three independent seeds:

  | seed / games | baseline avg / max / P(>=10) | PuyoAI12 avg / max / P(>=10) |
  |---|---|---|
  | 20260908, n=40, beam 8 | 7.43 / 11 / 3 | 7.98 / 11 / 5 |
  | 99999, n=30, beam 8 | 7.20 / 10 / 1 | 7.57 / 12 / 4 |
  | 555555, n=15, beam 16 | 8.13 / 11 / 4 | 8.20 / 10 / 1 |

  The third seed is a reminder that this is still a modest, noisy effect, not
  a solved problem -- see "Honest limitations" below.

- **(2) not implemented as a fixed-position template.** The existing 6x6
  `forms.cpp` matcher only covers a small fixed opening region and cannot
  describe a skeleton that spans the full board height a 13-15 chain needs.
  A general "packing efficiency" proxy (reward the fraction of filled cells
  that belong to a 2/3-group "module") was tried instead, as a stand-in for
  "matches an efficient large-chain skeleton" that works at any height. It
  was measured to trade the chain-count tail for more consistent mid-range
  results (same pattern as the over-wide version of (1)), so it was reverted.
  A real fixed-shape long-chain template library is still an open item; see
  "Still recommended" below.

- **(3) shipped, and safe on its own.** `shapePotential`'s height thresholds
  were shifted up by about one row (penalty now starts at maxH >= 12 instead
  of >= 12 with a harsher curve below it; see the diff for exact values).
  Tested in isolation (chain-reward and beam changes reverted to original) it
  matched-or-slightly-beat the baseline on every seed tried. A larger
  relaxation plus the packing-efficiency term from (2) was also tried
  together and measured worse (see above), so only this smaller shift
  shipped.

- **(4) implemented, measured harmful, reverted.** Reserving
  `beamWidth/8` beam slots for the best candidate of each distinct root
  column (so alternative constructions aren't pruned purely for a lower
  immediate score) sounded like a safe diversity mechanism, but tested in
  isolation (chain-reward and height changes reverted to original) it
  consistently lowered both the average and the max chain across every seed
  tried (e.g. 20260908/n=40/beam8: max chain 11 -> 9, P(>=10) 3 -> 0). At a
  small-to-moderate beam width, reserving slots for "merely diverse"
  candidates displaces genuinely strong ones more often than it rescues a
  future winner. This was not shipped. The production live-play default beam
  width was still raised from 12 to 20 (see `ai/ai.cpp`), independent of the
  reverted diversity mechanism, as a plain width increase measured no such
  problem.

- **(5) shipped, not benchmarked to convergence.** `tools/tuner.cpp`'s SPSA
  objective now rewards `maxChain^2` instead of a linear term and adds
  explicit bonuses for reaching 12+ and 15+ chains in a game, and its default
  `games`/`turns` were raised (6/35 -> 8/65) so a run has a realistic chance
  of a 12+ chain construction actually completing. Running SPSA to
  convergence on this new objective was not possible in the time available
  for this revision; the weights in `ai/evaluation/weights.cpp` are
  unchanged. This is the natural next step for continuing this research.

### Honest limitations

- All of the above was measured on 15-40 games per seed across 2-3 seeds,
  at beam widths (8-16) narrower than the 24-32 used for the original
  headline numbers, to fit within available compute. The reported combined
  effect (higher average max chain, more games reaching 8+, roughly flat-to-
  slightly-better P(>=10)) should be re-validated with the full 100 games x
  100 turns x beamWidth 24-32 protocol before being treated as final -- the
  third seed above already shows the effect is not uniform.
- Nothing here reaches 15 chains, or even 12, in these samples (atLeast12
  stayed 0 in every run). Reliably hitting 13-15 on a 6x12 board most likely
  needs one of: (a) a genuinely deeper search than 3 pieces (in tension with
  the human-information constraint this project has otherwise deliberately
  kept), (b) an explicit multi-stage template/skeleton the AI commits to and
  fills in rather than an emergent per-move evaluator, or (c) a learned
  value function (self-play RL) that can capture delayed-payoff structure
  the current hand-tuned static evaluator cannot represent well. All three
  are out of scope for this revision and are the most promising next steps.

# Survival-aware maximum-chain search

## Goal

This revision treats survival as a constrained secondary objective: the AI should
continue building a long chain whenever the board has enough mobility, but it
must retain an escape route before the danger column collapses.

The design is intentionally compatible with the existing human-information
constraint: the search still receives only the current pair plus the two visible
lookahead pairs.

## Changes

### 1. Two-step escape-route probe

`SurvivalHorizon` now records `bestNextSafeMoves` in addition to geometric
mobility. When the first horizon is narrow, the evaluator simulates the
following visible pair exactly and records the largest number of placements
that remain non-game-over.

This avoids a common false positive where a placement looks geometrically open
but the next pair has no actually safe placement after gravity/chain
resolution.

The survival cache key now includes both the current probed pair and the
following pair. This is important because the same board and current pair can
occur at different search depths with different next-next pairs.

### 2. Earlier survival sensing

The probe starts in a transition zone (`danger column >= 8` or maximum height
>= 10) rather than waiting until the board is almost dead. Comfortable boards
still receive essentially no survival preference.

### 3. Stronger collapse penalties

The survival curve is steeper at 0--3 safe moves. A board with zero or one
escape is therefore difficult to select merely because its chain-construction
score is attractive.

### 4. Survival reserve in the beam

When any candidate in a frontier enters the danger zone, up to one quarter of
the beam is reserved for candidates with the strongest escape mobility.

This is deliberately a reserve rather than a replacement for the chain
evaluator. The remaining beam continues to be ranked by the long-chain
construction objective.

### 5. Root candidates are fully probed

All root children are survival-probed instead of only the top-scoring subset.
A safe first move must not disappear before the search has had an opportunity
to compare its future mobility.

### 6. Final safety rescue

If the normal winner has at most one safe continuation for its root action,
the search may select an alternative root only when it does not reduce the
maximum chain already found and the alternative has a strong escape route.

## Benchmark observations

A deterministic comparison was run on the same seed (`20260908`), depth 3,
beam setting 24.

For 5 games x 60 turns:

| Version | Average max chain | Median | 10+ | 12+ | Games over |
|---|---:|---:|---:|---:|---:|
| Original uploaded version | 7.8 | 9 | 2/5 | 0/5 | 0/5 |
| Survival-aware revision | 9.8 | 10 | 4/5 | 1/5 | 0/5 |

This is a small sample and is not sufficient to claim a statistically
significant reduction in long-run game-over rate. A longer 100-game corpus is
still required for that conclusion. The important result for this revision is
that the survival mechanism did not require sacrificing the observed
maximum-chain performance on this short corpus.

The benchmark already reports the last five turns before a death, including
safe/geometric move counts, maximum height, danger-column height and occupied
cells. Those diagnostics should be used for the next tuning stage rather than
tuning survival weights from game-over counts alone.

## Research basis

The design follows several ideas that are particularly relevant to Puyo Puyo:

- Ikeda, Tomizawa, Viennot and Tanaka's 2012 CIG paper uses tree search,
  Potential Maximization and Template Matching for chain construction.
- Ama's public documentation emphasizes chain potential as a central
  evaluation signal and separates construction from the decision of when to
  ignite a chain.
- Takenaga and Shimada's work on single-player Puyo Puyo treats sustained
  playability as a fundamental requirement rather than optimizing chain length
  in isolation.
- Earlier Puyo Puyo AI work also demonstrates the usefulness of explicitly
  measuring survival as a separate objective.

The implementation therefore keeps latent chain potential primary while making
escape mobility an explicit constrained signal near collapse.

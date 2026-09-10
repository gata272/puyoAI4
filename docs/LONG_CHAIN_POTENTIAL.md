# Long-chain potential (PuyoAI11)

## Purpose

PuyoAI10 tended to concentrate around medium chains because immediate chain reward and lexicographic maximum-chain selection could favor a safe 7-8 chain over a quieter board that was still being constructed toward a larger chain.

PuyoAI11 keeps the three-pair information constraint and adds a static `longChainPotential` score. It is not a hidden-future predictor: it only inspects the current board and the current pair plus two visible lookahead pairs.

## Signals

- 2-puyo and 3-puyo same-color groups
- reachable top-of-column extension cells
- number of independent extendable groups
- moderate S-shaped height construction
- height pressure / near-top danger
- visible-queue compatibility
- existing ama human-form and trigger-route scores remain in the main evaluator

A 3-group receives more latent-build value than a 4+ group. This is intentional: a 4+ group can already fire and may be an attractive but premature cash-out, while an un-fired 3-group is a useful one-puyo extension target.

## Search changes

The global beam keeps two small elite subsets in addition to ordinary score ranking:

1. trigger-route elite
2. long-chain-potential elite

The final root decision is no longer lexicographically `maxChain` first. Actual chains still contribute strongly to the accumulated score and receive a separate chain-count bonus, but a quiet construction with much higher latent potential can survive and win.

## Information constraint

The evaluator and benchmark only receive:

- current pair
- next pair
- second next pair

No hidden queue is inspected. Unknown future colors are not treated as known facts.

## Validation

Native smoke tests and a dedicated long-chain-potential regression test are run in CI. A deterministic native benchmark was also run with seed `20260910`, 60 turns, depth 3, beam 24.

For the supplied PuyoAI4 baseline, the same benchmark settings produced average maximum chain 6.0 over 5 games. The PuyoAI11 experiment produced 7.2 over 5 games, with maximum 8 and two games reaching at least 8. This is an encouraging small-sample result, not a statistically conclusive claim; larger fixed seed corpora should be used before declaring the new evaluator superior.

# Chain Dependency Evaluation

The AI treats a large chain as a sequence of small chain units rather than as
one large same-colour blob.

## Core idea

A prepared exact-three group is a latent trigger. Hypothetically removing that
trigger and applying gravity reveals what becomes a four-or-more group. The
process is repeated wave by wave.

This supports structures such as:

`A -> B -> A -> C`

without assuming that every chain wave uses a new colour.

## Unit preference

A firing group of 4 or 5 is the preferred unit. Six is tolerated, and larger
units receive diminishing structural credit. This is a construction preference,
not a hard legality rule.

## Branching

Only simultaneous independent groups in the strongest candidate route are
penalized. Unrelated prepared triples elsewhere on the board do not create a
branch penalty by themselves.

## Search objective

The route length remains the dominant signal. Unit quality and dependency
continuity are tie-breaking construction signals. This prevents the evaluator
from preferring many small unrelated groups over one long sequential route.

## Current tuning

The default weights give exact-three latent units a stronger preference than the
previous geometry-only version: `chainUnit4=1100` and `chainUnit5=1400`. This is
still a soft signal; actual chain length remains the dominant search objective.

The production route evaluator is kept on the established chain-length scale.
Additional route-unit diagnostics are used only as same-length tie-breaking inside
the hypothetical route selection, so the change does not make large same-colour
blobs outrank a genuinely longer chain.

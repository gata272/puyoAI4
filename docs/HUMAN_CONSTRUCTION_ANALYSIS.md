# Human construction analysis

This revision uses the supplied human-recorded construction examples as qualitative
evidence. The notation `1r2b` means red placed in column 1 and blue in column 2;
the final two pairs are the next pieces at ignition, and the final number is the
observed chain count.

## Observations used by the evaluator

The examples show a repeated pattern that is more important than any single
height profile:

1. The board is built over many turns before ignition. The main chain is not
   repeatedly cashed out at the first available trigger.
2. A chain wave is commonly kept compact: 4-5 connected puyos are a useful
   target, while very large same-colour masses consume space without increasing
   the number of sequential waves.
3. The useful structure is sequential rather than parallel. A colour can be
   reused later, so the chain should be treated as events (`A -> B -> A -> C`)
   rather than as a graph whose nodes are unique colours.
4. Prepared groups are allowed to be latent. Exact-three anchors and 2+2
   preparations are valuable precisely because they are not yet four.
5. The surface tends to preserve usable landing surfaces. A perfectly flat
   board is not required; mild edge support and gentle slopes are compatible
   with long chains. Sharp central towers are much more damaging because they
   remove future landing positions.
6. The final ignition can depend on material that is not itself removable at
   the start. After the first wave and gravity, that material becomes the next
   chain wave. Therefore post-trigger exposure is a better signal than simply
   counting groups visible before ignition.
7. The two visible next pairs matter when deciding whether to continue
   construction. However, absence of a desired colour should not invalidate the
   current main chain; the AI needs a cleanup/holding move instead.

## Changes in this revision

### Handoff potential

For each exact-three anchor, the evaluator performs a bounded hypothetical
removal and gravity step. A single meaningful next firing wave receives a
bonus, especially when that next wave is a 4-5 group. Multiple simultaneous
waves are not rewarded as a replacement for a sequential route.

### Central peak

The evaluator now explicitly measures central towers relative to their nearby
flanks. This is separate from generic roughness/variance so that a board such as
`5 4 3 3 4 5` is not treated as equivalent to a board with a large central wall.

### Edge dead-end

An exact-three anchor at an edge is not automatically bad. It is penalized only
when the anchor has no neighboring escape/extension surface. This prevents the
AI from learning the wrong rule that all edge construction is undesirable.

### Future chain space

Usable landing cells near the current surface are rewarded, with a cap. Empty
space by itself is not valuable; space that can actually accept the next
construction is.

## Important implementation correction

The search previously constructed `candidate.score` before adding
`mainChainScore`. As a result, the accumulated beam score did not contain that
continuity signal even though it was computed. The score update now includes it.

The search also previously passed the current piece back into the visible
lookahead after that piece had already been placed. The lookahead now begins at
the next piece.

These two corrections are independent of the new human-style features and are
important for making the Main Chain policy actually influence search.

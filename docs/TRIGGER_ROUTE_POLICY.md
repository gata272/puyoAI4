# Trigger-route large-chain policy

PuyoAI11+ keeps the existing GTR, ama-style linear evaluation, 3-pair information limit, beam search, benchmark UI, and game-over fallback. The new construction policy is an additional beam-diversity mechanism rather than a replacement for the proven chain objective.

## 1. Prepared groups

The evaluator records exact 3-groups as primary latent anchors and 2-groups as secondary candidates. A route edge `B -> A` is recognized when removing a hypothetical exact-3 B trigger and applying gravity makes A a 4+ group.

This covers:

- 3+1: three connected A plus one A separated by the trigger's support.
- 2+2: two A pairs that become one 4+ group after the trigger is removed and gravity acts.

The route is evaluated without looking beyond the visible current pair plus two NEXT pairs.

## 2. Trigger-route diversity

The normal beam score remains the primary ranking signal. A small number of beam slots are reserved for candidates with strong prepared-group structure. This prevents a short-term shape from eliminating every candidate that contains a quiet A -> B -> C route.

The structural score is deliberately not added as a large global reward. It is used as a diversity/tie-break signal so that actual chain count remains the main objective.

## 3. Post-trigger chain-tail analysis

For final beam candidates, an exact-3 group is treated as a hypothetical trigger. The real simulator is then used to resolve the resulting chain. The evaluator records the resulting chain depth and newly fireable groups.

This is intended to capture the difference between:

- a board that merely contains many colored puyos, and
- a board where the upper/side material becomes a chain tail only after the trigger fires.

The expensive tail simulation is performed only on the final beam, not on every child, to keep thinking time practical.

## 4. Safety against premature firing

A 4+ group is already fireable, so it is not counted as a latent trigger. The structural tie-break only penalizes an immediate firing when a substantial prepared route exists around it.

## 5. Information constraint

The AI still uses only three visible pairs: current pair + NEXT 1 + NEXT 2. No hidden future queue is used by the trigger-route analysis.

## 6. Native performance

The native implementation remains C++20. Switching the whole simulator to another language would add a WASM integration risk without guaranteeing a faster end-to-end search. The hot search is compiled with `-O3 -flto` in the benchmark/build path, while the GitHub Pages WASM build already uses `-O3 -flto`.

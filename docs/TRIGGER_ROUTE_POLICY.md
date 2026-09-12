# Sequential chain-dependency path policy

The current large-chain policy keeps the existing GTR opening, ama-style linear evaluation, three-pair information limit, beam search, benchmark/debug UI, and game-over fallback. The main construction signal is now a **sequential chain-dependency path**.

## 1. Core idea

A useful construction is one where the removal of one trigger causes the next trigger/group to become removable on a later chain wave. The evaluator therefore prefers a long sequential path such as:

`A -> B -> A -> C -> D`

The repeated `A` is valid because nodes represent separate chain events/groups, not colours. The old four-colour limitation is intentionally gone.

## 2. Hypothetical trigger analysis

An exact-3 group is treated as a latent anchor. The evaluator temporarily removes that group and then uses `Simulator::resolveBoard()` with the production gravity/chain rules.

The number of resulting chain waves is the number of follow-up path steps. The latent anchor itself is counted as the first node, so the classic transfer motif has the form:

`B -> A`

and a three-wave continuation becomes:

`B -> A -> C -> ...`

This is a construction potential estimate, not an instruction to fire an exact-3 group in the live game.

## 3. Repeated colours

Colours are not graph nodes. Separate chain events may use the same colour:

`A1 -> B1 -> A2 -> C1`

This allows the evaluator to recognize nested constructions that return to a previously used colour.

## 4. No branch reward

Parallel same-wave removals do **not** increase the chain count. Therefore the policy does not reward graph branching.

For example, a structure equivalent to:

`A -> (B + C)`

is not treated as a two-step continuation. A small penalty is applied to genuinely parallel groups because they consume material without adding chain waves.

The primary signal is the **longest sequential path**.

## 5. Prepared groups

Exact-3 groups are primary latent anchors and exact-2 groups are weaker preparation material. A 4+ group is already fireable and is not rewarded as latent material.

The existing 3+1 / 2+2 idea is therefore interpreted as preparation for the next dependency step rather than as a collection of unrelated templates.

## 6. Post-trigger tail

The final beam still receives a more expensive post-trigger analysis. The board is compared before and after a hypothetical trigger, and material that becomes removable only after the trigger/gravity sequence contributes to the tail score.

This is intended to make the space above/around a future trigger useful instead of treating it as waste space.

## 7. Premature firing

If a valuable latent construction exists, turning it into an immediate 4+ group can destroy the route before it is finished. Such moves receive a penalty when the route/preparation value is substantial.

## 8. Missing colours

The visible queue is used as compatibility information, not as a hard requirement. If the desired next colour is absent, the AI may preserve the existing anchor and wait for a better opportunity.

## 9. Information constraint

The policy uses only the current pair plus NEXT 1 and NEXT 2. No hidden future queue information is used.

## 10. Search role

The sequential path score is combined with the existing evaluator inside Beam Search. Beam pruning explicitly reserves room for strong path candidates so a promising quiet construction is not discarded solely because it has not fired yet.

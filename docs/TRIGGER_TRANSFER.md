# Sequential Trigger Transfer

## Goal

The AI should construct a large chain using only the information available to a human: the current pair and the next two pairs.

The construction is viewed as a sequence of dependencies rather than a fixed colour template.

## Marked trigger

An exact three-puyo group is a latent trigger/anchor. The AI asks what would happen if that group were removed and the remaining board were resolved using the real simulator.

## Sequential dependency

If removing a trigger causes another group to become removable on a later chain wave, the two events form a dependency path.

Examples:

`B -> A`

`C -> B -> A`

`A -> B -> A -> C`

The last example is important: the same colour may occur at multiple stages. Each occurrence is a different chain event, so the path is not limited to four colours.

## 3+1 and related structures

A structure such as three connected A puyos plus another A separated by a trigger is useful because removing the trigger can make A fireable. Horizontal and vertical variants are handled by the same simulator-based rule instead of separate geometry-specific cases.

Exact-2 groups are weaker preparation material. They are useful when they can become part of the next dependency step without immediately firing.

## No branch objective

The AI does not reward branching merely because several groups disappear together. Multiple groups on the same wave consume material but do not add multiple chain counts.

The main structural objective is therefore:

**make one chain wave cause one useful next chain wave, and repeat this for as many stages as possible.**

## Post-trigger tail

The evaluator also examines the board after a hypothetical trigger. Puyos that are not removable before the trigger but become removable after the trigger and subsequent gravity are treated as useful chain-tail material.

## Search

The path signal is combined with the existing board evaluator, GTR construction, long-chain potential, premature-trigger protection, and three-pair beam search. The benchmark and debug systems remain unchanged.

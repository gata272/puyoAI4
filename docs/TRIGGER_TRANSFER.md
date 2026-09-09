# Persistent Trigger Transfer

## Goal

The AI does not attempt to see a long future directly. It sees only three pairs:

1. current pair
2. next pair
3. next-next pair

The long-chain objective is converted into a repeated local construction problem.

## Marked trigger

An exact three-puyo group is a trigger candidate. The AI estimates which trigger is most valuable by hypothetically removing that group and applying the real simulator's gravity and chain-resolution rules.

The trigger is then treated as an anchor. A move that destroys a strong anchor without producing a chain is penalized.

## Trigger transfer

If an exact-3 B group exists such that removing B causes an A group to become a 4+-group after gravity, the board contains a dependency:

`B -> A`

This captures both the vertical and horizontal forms discussed during development. A sequence such as

`D -> C -> B -> A`

is therefore represented as a four-level trigger route.

The important distinction is that the AI is not required to fire A immediately. It can preserve A while building B, then preserve B while building C, and so on.

## Missing desired colors

The visible queue is used only for compatibility. If the next predecessor color is absent, the trigger anchor remains valuable. The evaluator therefore permits a waiting move that preserves the anchor rather than forcing a bad placement merely to use the current pair.

## Implementation notes

- `trigger_route.cpp` contains the dependency detector and structural scoring.
- `Simulator::resolveBoard()` reuses the exact production chain resolver for hypothetical trigger tests.
- `EvaluationContext::lookahead` contains at most three visible pairs.
- `BeamSearch` is capped at depth 3 in the production configuration.
- The opening GTR planner remains unchanged.

This is intentionally a research implementation. The dependency graph is a structural approximation of the eventual chain and should be evaluated against a larger fixed queue corpus before claiming superiority over the previous maximum-chain beam search.

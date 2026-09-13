# Main Chain Policy

The search keeps a single **main chain plan** for each beam node instead of
rewarding unrelated triples equally.

## Plan

A plan starts from an exact-three prepared trigger and records the colours of
successive chain waves produced by a hypothetical trigger. Repeated colours
are allowed, so routes such as `A -> B -> A -> C` are valid.

The first colour is the prepared trigger. For example, if clearing `B` makes
an `AAA` group disappear on the next wave, the route begins `B -> A`.

## Search behavior

For every child node the AI:

1. analyzes the strongest current sequential route;
2. compares it with the parent's route;
3. rewards preserving the same prefix;
4. gives a strong bonus when the route is extended;
5. penalizes abandoning the route while it is still viable;
6. after an actual chain fires, advances the remembered route by the number
   of real waves so a legitimate `B -> A` transition is not treated as a
   broken plan;
7. allows a small cleanup bonus only when the main route remains intact.

Parallel groups are not treated as extra chain length. They are recorded as
branch waves and receive a secondary penalty, keeping the objective focused
on one sequential chain.

## Why this is different from static evaluation

`triggerRouteLength()` and `longChainPotential()` still describe the current
board. `MainChainPlan` adds temporal continuity: the beam node remembers what
chain it was trying to build and evaluates the next board against that plan.
This makes a three-pair search capable of making locally quiet moves whose
purpose is to preserve and extend one long-chain blueprint.

The policy is deliberately heuristic rather than a hard lock. If the plan
becomes impossible, the beam can abandon it and establish a new route.

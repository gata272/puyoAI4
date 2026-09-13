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


## 2026-09 construction policy update

The main-chain policy is intentionally a **soft construction layer**, not a second
search objective that can overwhelm actual chain results.

### 1. Delay the cash-out

An exact-three group is treated as a prepared trigger. The AI does not equate
"can be triggered" with "should be triggered". The route is preferred when it
can continue to another sequential wave, while unrelated prepared triples are
only a weak risk.

### 2. One physical chain spine

`MainChainPlan` remains a single sequential route. Repeated colours are legal,
so routes such as `A -> B -> A -> C` are preserved. Same-wave independent groups
are penalized rather than rewarded as a substitute for depth.

### 3. Workspace and geometry

The construction score considers:

- route length;
- exact-3 and exact-2 prepared groups;
- remaining visible workspace;
- the route anchor's distance from the center;
- available vertical space around the anchor;
- a **soft** edge-wall quality term.

Edge height is therefore not rewarded by itself. A high edge is useful only when
it behaves like a wall while the central construction area remains open.

### 4. Search integration

Construction geometry is applied primarily at final ranking with a small
coefficient. It is deliberately not accumulated as a full per-depth reward:
otherwise a small geometric advantage compounds through Beam Search and can
cause the AI to abandon a genuinely longer chain.

The existing chain reward, route score, and Main Chain continuity remain the
dominant signals.

### 5. Safety

The construction metrics are bounded and use the same `Board` dimensions as the
simulator. They do not change placement legality. If every legal placement is
a game-over placement, the existing fallback behavior remains responsible for
choosing the least-bad placement.

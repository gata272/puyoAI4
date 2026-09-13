# Board Geometry Policy

This version treats board geometry as a means to preserve future chain-construction space, not as a generic flatness objective.

## Central peak

A central peak is penalized when columns 3-4 are substantially higher than the four outer columns. This specifically targets the recurring pattern where columns 2-5 become a wall and force the active trigger toward an edge.

## Edge walls

Higher edges are allowed and can be useful. The AI does not simply punish high edge columns; it prefers an edge-high/center-lower profile when the edges still leave usable space.

## Trigger expansion space

For exact-three groups, the evaluator counts physically reachable landing cells adjacent to the group and gives extra value to having both horizontal escape directions. This approximates the space required for repeated B->A, C->B, A->C-style transfers.

## Edge dead ends

An exact-three trigger near an edge is penalized only when its available expansion directions are poor. An edge trigger with useful construction space remains valid.

## Future construction space

The evaluator rewards receiving/tail space around the current construction while avoiding a blanket reward for an empty board.

These terms are secondary to actual chain count and sequential chain-dependency potential. They are intended to keep the main chain alive long enough to reach larger chain counts.
